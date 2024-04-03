//
// Created by jplc on 4/2/24.
//

#include "OpenSqliteHo.h"
#include "bridge.h"
#include "macros.h"
#include "types.h"

namespace jsi = facebook::jsi;
using std::string;

namespace opsqlite {

jsi::Function OpenSqliteHo::open(jsi::Runtime &rt,
                                 const std::string &basePath) {
  return HOSTFN("open", 3) {
    string errMsg;
    if (isParametersNumberNotOk(errMsg, count)) {
      throw std::runtime_error(errMsg);
    }

    jsi::Object options = args[0].asObject(rt);
    std::string dbName = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = getPath(rt, options, basePath);
    std::string encryptionKey = OpenSqliteHo::getEncryptionKey(rt, options);

#ifdef OP_SQLITE_USE_SQLCIPHER
    if (encryptionKey.empty()) {
      throw std::runtime_error(
          "[OP SQLite] using SQLCipher encryption key is required");
    }
    //      TODO(osp) find a way to display the yellow box from c++
    BridgeResult result = opsqlite_open(dbName, path, encryptionKey);
#else
    // if (!encryptionKey.empty()) {
    //   //  RCTLogWarn(@"Your message")
    //   throw std::runtime_error("[OP SQLite] SQLCipher is not enabled, "
    //                            "encryption key is not allowed");
    // }
    BridgeResult result = opsqlite_open(dbName, path);
#endif

    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }

    return {};
  });
}

bool OpenSqliteHo::isParametersNumberNotOk(string &msg, int count) {
  if (count == 0) {
    msg = "[op-sqlite][open] database name is required";
    return true;
  }
  return false;
}

string OpenSqliteHo::getPath(jsi::Runtime &rt, const jsi::Object &options,
                             const string &basePath) {
  string path = basePath;
  string location = getLocation(rt, options);
  if (!location.empty()) {
    if (location == ":memory:") {
      path = ":memory:";
    } else if (location.rfind("/", 0) == 0) {
      path = location;
    } else {
      path = path + "/" + location;
    }
  }
  return path;
}

string OpenSqliteHo::getLocation(jsi::Runtime &rt, const jsi::Object &options) {
  string location = "";
  if (options.hasProperty(rt, "location")) {
    location = options.getProperty(rt, "location").asString(rt).utf8(rt);
  }
  return location;
}

string OpenSqliteHo::getEncryptionKey(jsi::Runtime &rt,
                                      const jsi::Object &options) {
  string encryptionKey = "";
  if (options.hasProperty(rt, "encryptionKey")) {
    encryptionKey =
        options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
  }
  return encryptionKey;
}

} // namespace opsqlite
