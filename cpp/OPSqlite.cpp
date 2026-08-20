#include "OPSqlite.hpp"
#include "OPDatabase.hpp"
#include "DumbHostObject.hpp"
#include "OPThreadPool.hpp"
#ifdef OP_SQLITE_USE_LIBSQL
#include "libsql/bridge.hpp"
#else
#include "bridge.hpp"
#endif
#include "logs.h"
#include "macros.hpp"
#include "utils.hpp"
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

std::string _base_path;
std::string _sqlite_vec_path;
std::shared_ptr<react::CallInvoker> invoker;
std::shared_ptr<std::atomic<bool>> generation_alive;

// Each platform module calls its own invalidate() lifecycle hook when React
// Native tears down the JS context (CodePush/Hot Reload, or any other
// runtime teardown) -- OPSQLiteModule.invalidate() on Android, which reaches
// this function through JNI's clearStateNativeJsi (see android/cpp-adapter.cpp),
// and -[OPSQLite invalidate] on iOS (see ios/OPSQLite.mm). Each currently
// open OPDatabase cleans itself up independently -- OPDatabase's own
// invalidate() (interrupt + drain + close), called from its destructor,
// already runs whenever the JS runtime destroys it, so there's no registry to
// walk here. All THIS invalidate() needs to do is mark THIS generation dead so
// in-flight async work drops its result instead of resolving into whichever
// runtime replaces it.
void invalidate(const std::shared_ptr<std::atomic<bool>> &generation_alive) {
  if (generation_alive != nullptr) {
    generation_alive->store(false);
  }
}

std::shared_ptr<std::atomic<bool>>
install(jsi::Runtime &rt, const std::shared_ptr<react::CallInvoker> &invoker,
        const char *base_path, const char *sqlite_vec_path) {

  _base_path = std::string(base_path);
  _sqlite_vec_path = std::string(sqlite_vec_path);
  opsqlite::invoker = invoker;

  // Also returned to the caller: DBs opened by this generation shadow-copy
  // the global at construction time (see OPDatabase.hpp), while
  // invalidate() needs the shared_ptr handed back directly so it flips THIS
  // generation's flag even if a newer, overlapping generation's install()
  // has already reassigned the global.
  auto local_generation_alive = std::make_shared<std::atomic<bool>>(true);
  opsqlite::generation_alive = local_generation_alive;

  auto open = HFN0 {
    jsi::Object options = args[0].asObject(rt);
    std::string name = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(_base_path);
    std::string location;
    std::string encryption_key;
    bool readOnly = false;
    bool failOnCreate = false;

    if (options.hasProperty(rt, "location")) {
      location = options.getProperty(rt, "location").asString(rt).utf8(rt);
    }

    if (options.hasProperty(rt, "encryptionKey")) {
      encryption_key =
          options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
    }

    if (options.hasProperty(rt, "readOnly")) {
      readOnly = options.getProperty(rt, "readOnly").asBool();
    }

    if (options.hasProperty(rt, "failOnCreate")) {
      failOnCreate = options.getProperty(rt, "failOnCreate").asBool();
    }

    if (!location.empty()) {
      if (location == ":memory:") {
        path = ":memory:";
      } else if (location.rfind('/', 0) == 0) {
        path = location;
      } else {
        path = path + "/" + location;
      }
    }

    jsi::Object js_db(rt);
    std::shared_ptr<OPDatabase> db = std::make_shared<OPDatabase>(
        rt, js_db, path, name, path, readOnly, failOnCreate, encryption_key);
    js_db.setNativeState(rt, db);
    return js_db;
  });

  auto is_sqlcipher = HFN(=) {
#ifdef OP_SQLITE_USE_SQLCIPHER
    return true;
#else
    return false;
#endif
  });

  auto is_ios_embedded = HFN(=) {
#ifdef OP_SQLITE_USE_PHONE_VERSION
    return true;
#else
    return false;
#endif
  });

  auto is_libsql = HFN(=) {
#ifdef OP_SQLITE_USE_LIBSQL
    return true;
#else
    return false;
#endif
  });

  auto is_turso = HFN(=) {
#ifdef OP_SQLITE_USE_TURSO
    return true;
#else
    return false;
#endif
  });

#if defined(OP_SQLITE_USE_LIBSQL) || defined(OP_SQLITE_USE_TURSO)
  auto open_remote = HFN(=) {
    jsi::Object options = args[0].asObject(rt);

    std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);

    std::string auth_token =
      options.getProperty(rt, "authToken").asString(rt).utf8(rt);

    jsi::Object js_db(rt);
#ifdef OP_SQLITE_USE_LIBSQL
    std::shared_ptr<OPDatabase> db =
        std::make_shared<OPDatabase>(rt, js_db, url, auth_token);
#else
    std::string path = std::string(_base_path);
    std::shared_ptr<OPDatabase> db =
        std::make_shared<OPDatabase>(rt, js_db, url, auth_token, path);
#endif

    js_db.setNativeState(rt, db);
    return js_db;
  });

  auto open_sync = HFN(=) {
    jsi::Object options = args[0].asObject(rt);
    std::string name = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = std::string(_base_path);
    std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);
    std::string auth_token =
      options.getProperty(rt, "authToken").asString(rt).utf8(rt);

    int sync_interval = 0;
    if (options.hasProperty(rt, "libsqlSyncInterval")) {
      sync_interval = static_cast<int>(
          options.getProperty(rt, "libsqlSyncInterval").asNumber());
    }

    bool offline = false;
    if (options.hasProperty(rt, "libsqlOffline")) {
      offline = options.getProperty(rt, "libsqlOffline").asBool();
    }

    std::string encryption_key;
    if (options.hasProperty(rt, "encryptionKey")) {
      encryption_key =
        options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
    }

    std::string remote_encryption_key;
    if (options.hasProperty(rt, "remoteEncryptionKey")) {
      remote_encryption_key =
        options.getProperty(rt, "remoteEncryptionKey").asString(rt).utf8(rt);
    }

    std::string location;
    if (options.hasProperty(rt, "location")) {
      location = options.getProperty(rt, "location").asString(rt).utf8(rt);
    }
    if (!location.empty()) {
      if (location == ":memory:") {
        path = ":memory:";
      } else if (location.rfind('/', 0) == 0) {
        path = location;
      } else {
        path = path + "/" + location;
      }
    }

    jsi::Object js_db(rt);
  #ifdef OP_SQLITE_USE_LIBSQL
    std::shared_ptr<OPDatabase> db = std::make_shared<OPDatabase>(
      rt, js_db, name, path, url, auth_token, sync_interval, offline,
      encryption_key, remote_encryption_key);
  #else
    (void)sync_interval;
    (void)offline;

    std::shared_ptr<OPDatabase> db = std::make_shared<OPDatabase>(
      rt, js_db, name, path, url, auth_token, remote_encryption_key);
  #endif

    js_db.setNativeState(rt, db);
    return js_db;
  });
#endif

  jsi::Object module = jsi::Object(rt);
  module.setProperty(rt, "open", std::move(open));
  module.setProperty(rt, "isSQLCipher", std::move(is_sqlcipher));
  module.setProperty(rt, "isLibsql", std::move(is_libsql));
  module.setProperty(rt, "isTurso", std::move(is_turso));
  module.setProperty(rt, "isIOSEmbedded", std::move(is_ios_embedded));
#if defined(OP_SQLITE_USE_LIBSQL) || defined(OP_SQLITE_USE_TURSO)
  module.setProperty(rt, "openRemote", std::move(open_remote));
  module.setProperty(rt, "openSync", std::move(open_sync));
#endif

  rt.global().setProperty(rt, "__OPSQLiteProxy", std::move(module));

  return local_generation_alive;
}

void expoUpdatesWorkaround(const char *base_path) {
#ifdef OP_SQLITE_USE_LIBSQL
  std::string path = std::string(base_path);
  // Open a DB before anything else so that expo-updates does not mess up the
  // configuration
  opsqlite_libsql_open("__dummy", path, false);
#endif
}

} // namespace opsqlite
