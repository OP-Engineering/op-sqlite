//
// Created by jplc on 4/2/24.
//

#include "DbHostObject.h"
#include "bridge.h"
#include "macros.h"
#include "types.h"
#include "validators/DbOpenValidator.h"
#include <string>

using opsqlite::validators::DbOpenValidator;

namespace opsqlite {

const std::string DbHostObject::F_OPEN = "open";
const int DbHostObject::F_OPEN_ARGS_COUNT = 3;

jsi::Value DbHostObject::get(jsi::Runtime &runtime,
                             const jsi::PropNameID &propNameId) {
  auto methodName = propNameId.utf8(runtime);
  if (methodName == F_OPEN) {
    return DbHostObject::open;
  }
  return nullptr;
}

void DbHostObject::set(jsi::Runtime &runtime, const jsi::PropNameID &propNameId,
                       const jsi::Value &value) {
  // no attributes to set at moment.
}

std::vector<jsi::PropNameID>
DbHostObject::getPropertyNames(jsi::Runtime &runtime) {
  std::vector<jsi::PropNameID> properties;
  properties.push_back(jsi::PropNameID::forAscii(runtime, F_OPEN));
  return properties;
}

jsi::Function DbHostObject::open(jsi::Runtime &rt,
                                 const std::string &basePath) {
  return HOSTFN(F_OPEN, F_OPEN_ARGS_COUNT) {
    std::string errMsg;
    if (DbOpenValidator::isParametersNumberNotOk(errMsg, count)) {
      throw std::runtime_error(errMsg);
    }

    jsi::Object options = args[0].asObject(rt);
    std::string dbName = options.getProperty(rt, "name").asString(rt).utf8(rt);
    std::string path = DbOpenValidator::getPath(rt, options, basePath);
    std::string encryptionKey = DbOpenValidator::getEncryptionKey(rt, options);

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

} // namespace opsqlite
