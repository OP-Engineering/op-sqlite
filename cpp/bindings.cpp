#include "bindings.h"
#include "DBHostObject.h"
#include "DumbHostObject.h"
#include "OPThreadPool.h"
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
std::string _sqlite_vec_path;
std::vector<std::shared_ptr<DBHostObject>> dbs;

// React native will try to clean the module on JS context invalidation
// (CodePush/Hot Reload) The clearState function is called
void invalidate() {
    for (const auto &db : dbs) {
        db->invalidate();
    }

    // Clear our existing vector of shared pointers so they can be garbage
    // collected
    dbs.clear();
}

void install(jsi::Runtime &rt,
             const std::shared_ptr<react::CallInvoker> &invoker,
             const char *base_path, const char *crsqlite_path,
             const char *sqlite_vec_path) {
    _base_path = std::string(base_path);
    _crsqlite_path = std::string(crsqlite_path);
    _sqlite_vec_path = std::string(sqlite_vec_path);

    auto open = HOST_STATIC_FN("open") {
        jsi::Object options = args[0].asObject(rt);
        std::string name =
            options.getProperty(rt, "name").asString(rt).utf8(rt);
        std::string path = std::string(_base_path);
        std::string location;
        std::string encryption_key;

        if (options.hasProperty(rt, "location")) {
            location =
                options.getProperty(rt, "location").asString(rt).utf8(rt);
        }

        if (options.hasProperty(rt, "encryptionKey")) {
            encryption_key =
                options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
        }

#ifdef OP_SQLITE_USE_SQLCIPHER
        if (encryption_key.empty()) {
            log_to_console(rt, "Encryption key is missing for SQLCipher");
        }
#endif

        if (!location.empty()) {
            if (location == ":memory:") {
                path = ":memory:";
            } else if (location.rfind('/', 0) == 0) {
                path = location;
            } else {
                path = path + "/" + location;
            }
        }

        std::shared_ptr<DBHostObject> db = std::make_shared<DBHostObject>(
            rt, path, invoker, name, path, _crsqlite_path, _sqlite_vec_path,
            encryption_key);
        dbs.emplace_back(db);
        return jsi::Object::createFromHostObject(rt, db);
    });

    auto is_sqlcipher = HOST_STATIC_FN("isSQLCipher") {
#ifdef OP_SQLITE_USE_SQLCIPHER
        return true;
#else
        return false;
#endif
    });

    auto is_ios_embedded = HOST_STATIC_FN("isIOSEmbedded") {
#ifdef OP_SQLITE_USE_PHONE_VERSION
        return true;
#else
        return false;
#endif
    });

    auto is_libsql = HOST_STATIC_FN("isLibsql") {
#ifdef OP_SQLITE_USE_LIBSQL
        return true;
#else
        return false;
#endif
    });

#ifdef OP_SQLITE_USE_LIBSQL
    auto open_remote = HOST_STATIC_FN("openRemote") {
        jsi::Object options = args[0].asObject(rt);
        std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);
        std::string auth_token =
            options.getProperty(rt, "authToken").asString(rt).utf8(rt);

        std::shared_ptr<DBHostObject> db =
            std::make_shared<DBHostObject>(rt, url, auth_token, invoker);
        return jsi::Object::createFromHostObject(rt, db);
    });

    auto open_sync = HOST_STATIC_FN("openSync") {
        jsi::Object options = args[0].asObject(rt);
        std::string name =
            options.getProperty(rt, "name").asString(rt).utf8(rt);
        std::string path = std::string(_base_path);
        std::string url = options.getProperty(rt, "url").asString(rt).utf8(rt);
        std::string auth_token =
            options.getProperty(rt, "authToken").asString(rt).utf8(rt);

        int sync_interval = 0;
        if (options.hasProperty(rt, "libsqlSyncInterval")) {
            sync_interval = static_cast<int>(
                options.getProperty(rt, "syncInterval").asNumber());
        }

        bool offline = false;
        if (options.hasProperty(rt, "libsqlOffline")) {
            offline = options.getProperty(rt, "libsqlOffline").asBool();
        }

        std::string location;
        if (options.hasProperty(rt, "location")) {
            location =
                options.getProperty(rt, "location").asString(rt).utf8(rt);
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
            rt, invoker, name, path, url, auth_token, sync_interval, offline);
        return jsi::Object::createFromHostObject(rt, db);
    });
#endif

    jsi::Object module = jsi::Object(rt);
    module.setProperty(rt, "open", std::move(open));
    module.setProperty(rt, "isSQLCipher", std::move(is_sqlcipher));
    module.setProperty(rt, "isLibsql", std::move(is_libsql));
    module.setProperty(rt, "isIOSEmbedded", std::move(is_ios_embedded));
#ifdef OP_SQLITE_USE_LIBSQL
    module.setProperty(rt, "openRemote", std::move(open_remote));
    module.setProperty(rt, "openSync", std::move(open_sync));
#endif

    rt.global().setProperty(rt, "__OPSQLiteProxy", std::move(module));
}

void expoUpdatesWorkaround(const char *base_path) {
#ifdef OP_SQLITE_USE_LIBSQL
    std::string path = std::string(base_path);
    // Open a DB before anything else so that expo-updates does not mess up the
    // configuration
    opsqlite_libsql_open("__dummy", path, "");
#endif
}

} // namespace opsqlite
