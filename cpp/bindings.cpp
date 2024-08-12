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

std::string _base_path;
std::string _crsqlite_path;
std::shared_ptr<react::CallInvoker> _invoker;
std::shared_ptr<ThreadPool> thread_pool = std::make_shared<ThreadPool>();

// React native will try to clean the module on JS context invalidation
// (CodePush/Hot Reload) The clearState function is called and we use this flag
// to prevent any ongoing operations from continuing work and can return early
bool invalidated = false;

void clearState() {
  invalidated = true;

#ifdef OP_SQLITE_USE_LIBSQL
  opsqlite_libsql_close_all();
#else
  opsqlite_close_all();
#endif

  // We then join all the threads before the context gets invalidated
  thread_pool->restartPool();
}

void install(jsi::Runtime &rt, std::shared_ptr<react::CallInvoker> invoker,
             const char *base_path, const char *crsqlite_path) {
  invalidated = false;
  _base_path = std::string(base_path);
  _crsqlite_path = std::string(crsqlite_path);
  _invoker = invoker;

  auto open = HOSTFN("open") {
    jsi::Object options = args[0].asObject(rt);
    std::string name = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(_base_path);
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

    std::shared_ptr<DBHostObject> db =
        std::make_shared<DBHostObject>(rt, path, invoker, thread_pool, name,
                                       path, _crsqlite_path, encryptionKey);
    return jsi::Object::createFromHostObject(rt, db);
  });

  auto is_sqlcipher = HOSTFN("isSQLCipher") {
#ifdef OP_SQLITE_USE_SQLCIPHER
    return true;
#else
    return false;
#endif
  });

  auto is_libsql = HOSTFN("isLibsql") {
#ifdef OP_SQLITE_USE_LIBSQL
    return true;
#else
    return false;
#endif
  });

#ifdef OP_SQLITE_USE_LIBSQL
  auto open_remote = HOSTFN("openRemote", 1) {
    jsi::Object options = args[0].asObject(rt);
    std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);
    std::string auth_token =
        options.getProperty(rt, "authToken").asString(rt).utf8(rt);

    std::shared_ptr<DBHostObject> db = std::make_shared<DBHostObject>(
        rt, url, auth_token, invoker, thread_pool);
    return jsi::Object::createFromHostObject(rt, db);
  });

  auto open_sync = HOSTFN("openSync", 1) {
    jsi::Object options = args[0].asObject(rt);
    std::string name = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(_base_path);
    std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);
    std::string auth_token =
        options.getProperty(rt, "authToken").asString(rt).utf8(rt);
    int sync_interval =
        static_cast<int>(options.getProperty(rt, "syncInterval").asNumber());
    std::string location;

    if (options.hasProperty(rt, "location")) {
      location = options.getProperty(rt, "location").asString(rt).utf8(rt);
    }

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
        rt, invoker, thread_pool, name, path, url, auth_token, sync_interval);
    return jsi::Object::createFromHostObject(rt, db);
  });
#endif

  jsi::Object module = jsi::Object(rt);
  module.setProperty(rt, "open", std::move(open));
  module.setProperty(rt, "isSQLCipher", std::move(is_sqlcipher));
  module.setProperty(rt, "isLibsql", std::move(is_libsql));
#ifdef OP_SQLITE_USE_LIBSQL
  module.setProperty(rt, "openRemote", std::move(open_remote));
  module.setProperty(rt, "openSync", std::move(open_sync));
#endif

  rt.global().setProperty(rt, "__OPSQLiteProxy", std::move(module));
}

} // namespace opsqlite
