//
// Created by jplc on 4/2/24.
//

#include "DbHostObject.h"
#include "bridge.h"
#include "macros.h"
#include "types.h"
#include "validators/DbAttachValidator.h"
#include "validators/DbDetachValidator.h"
#include "validators/DbOpenValidator.h"
#include <string>

namespace opsqlite {

const std::string DbHostObject::F_OPEN = "open";
const int DbHostObject::F_OPEN_ARGS_COUNT = 3;
const std::string DbHostObject::F_ATTACH = "attach";
const int DbHostObject::F_ATTACH_ARGS_COUNT = 4;
const std::string DbHostObject::F_DETACH = "detach";
const int DbHostObject::F_DETACH_ARGS_COUNT = 2;

jsi::Value DbHostObject::get(jsi::Runtime &runtime,
                             const jsi::PropNameID &propNameId) {
  auto methodName = propNameId.utf8(runtime);
  if (methodName == F_OPEN) {
    return &DbHostObject::open;
  } else if (methodName == F_ATTACH) {
    return &DbHostObject::attach;
  } else if (methodName == F_DETACH) {
    return &DbHostObject::detach;
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
  properties.push_back(jsi::PropNameID::forAscii(runtime, F_ATTACH));
  properties.push_back(jsi::PropNameID::forAscii(runtime, F_DETACH));
  return properties;
}

jsi::Function DbHostObject::open(jsi::Runtime &rt,
                                 const std::string &basePath) {
  using opsqlite::validators::DbOpenValidator;
  return HOSTFN(F_OPEN, F_OPEN_ARGS_COUNT) {
    std::string errMsg;
    if (DbOpenValidator::invalidArgsNumber(errMsg, count)) {
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

jsi::Function DbHostObject::attach(jsi::Runtime &rt,
                                   const std::string &basePath) {
  using opsqlite::validators::DbAttachValidator;
  return HOSTFN(F_ATTACH, F_ATTACH_ARGS_COUNT) {
    std::string errMsg;
    if (DbAttachValidator::invalidArgsNumber(errMsg, count)) {
      throw jsi::JSError(rt, errMsg);
    }
    if (DbAttachValidator::noStringArgs(errMsg, args[0], args[1], args[2])) {
      throw jsi::JSError(rt, errMsg);
      return {};
    }

    std::string tempDocPath = std::string(basePath);
    if (DbAttachValidator::locationArgDefined(count, args[3])) {
      if (DbAttachValidator::locationArgIsNotString(errMsg, args[3])) {
        throw std::runtime_error(errMsg);
      }
      tempDocPath = tempDocPath + "/" + args[3].asString(rt).utf8(rt);
    }

    std::string dbName = args[0].asString(rt).utf8(rt);
    std::string databaseToAttach = args[1].asString(rt).utf8(rt);
    std::string alias = args[2].asString(rt).utf8(rt);
    BridgeResult result =
        opsqlite_attach(dbName, tempDocPath, databaseToAttach, alias);

    if (result.type == SQLiteError) {
      throw std::runtime_error(result.message);
    }

    return {};
  });
}

jsi::Function DbHostObject::detach(jsi::Runtime &rt) {
  using opsqlite::validators::DbDetachValidator;
  return HOSTFN(F_DETACH, F_DETACH_ARGS_COUNT) {
    std::string errMsg;
    if (DbDetachValidator::invalidArgsNumber(errMsg, count)) {
      throw std::runtime_error(errMsg);
    }
    if (DbDetachValidator::noStringArgs(errMsg, args[0], args[1])) {
      throw std::runtime_error(errMsg);
      return {};
    }

    std::string dbName = args[0].asString(rt).utf8(rt);
    std::string alias = args[1].asString(rt).utf8(rt);
    BridgeResult result = opsqlite_detach(dbName, alias);

    if (result.type == SQLiteError) {
      throw jsi::JSError(rt, result.message.c_str());
    }

    return {};
  });
}

} // namespace opsqlite
