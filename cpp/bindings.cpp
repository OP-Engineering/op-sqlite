#include "bindings.h"
#include "DBHostObject.h"
#include "DumbHostObject.h"
#include "ThreadPool.h"
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.h"
#else
#include "bridge.h"
#endif
#include "logs.h"
#include "macros.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

std::string basePath;
std::string crsqlitePath;
std::shared_ptr<react::CallInvoker> invoker;
std::shared_ptr<ThreadPool> thread_pool = std::make_shared<ThreadPool>();

// React native will try to clean the module on JS context invalidation
// (CodePush/Hot Reload) The clearState function is called and we use this flag
// to prevent any ongoing operations from continuing work and can return early
bool invalidated = false;

void clearState() {
  invalidated = true;
  // Will terminate all operations and database connections
  //  opsqlite_close_all();
  // We then join all the threads before the context gets invalidated
  thread_pool->restartPool();
}

void install(jsi::Runtime &rt,
             std::shared_ptr<react::CallInvoker> js_call_invoker,
             const char *doc_path, const char *_crsqlitePath) {

  invalidated = false;
  basePath = std::string(doc_path);
  crsqlitePath = std::string(_crsqlitePath);
  invoker = js_call_invoker;

  auto open = HOSTFN("open", 3) {
    if (count == 0) {
      throw std::runtime_error("[op-sqlite][open] database name is required");
    }

    jsi::Object options = args[0].asObject(rt);
    std::string dbName = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(basePath);
    std::string location;
    std::string encryptionKey;

    if (options.hasProperty(rt, "location")) {
      location = options.getProperty(rt, "location").asString(rt).utf8(rt);
    }

    if (options.hasProperty(rt, "encryptionKey")) {
      encryptionKey =
          options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
    }

#ifdef OP_SQLITE_USE_SQLCIPHER
    if (encryptionKey.empty()) {
      throw std::runtime_error(
          "[OP SQLite] using SQLCipher encryption key is required");
    }
#endif

    if (!location.empty()) {
      if (location == ":memory:") {
        path = ":memory:";
      } else if (location.rfind("/", 0) == 0) {
        path = location;
      } else {
        path = path + "/" + location;
      }
    }

    std::shared_ptr<DBHostObject> db = std::make_shared<DBHostObject>(
        rt, basePath, invoker, thread_pool, dbName, path, crsqlitePath,
        encryptionKey);
    return jsi::Object::createFromHostObject(rt, db);
  });

  auto is_sqlcipher = HOSTFN("isSQLCipher", 0) {
#ifdef OP_SQLITE_USE_SQLCIPHER
    return true;
#else
    return false;
#endif
  });

  auto is_libsql = HOSTFN("isLibsql", 0) {
#ifdef OP_SQLITE_USE_LIBSQL
    return true;
#else
    return false;
#endif
  });

  jsi::Object module = jsi::Object(rt);
  module.setProperty(rt, "open", std::move(open));
  module.setProperty(rt, "isSQLCipher", std::move(is_sqlcipher));
  module.setProperty(rt, "isLibsql", std::move(is_libsql));

  rt.global().setProperty(rt, "__OPSQLiteProxy", std::move(module));
}

} // namespace opsqlite
