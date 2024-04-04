#include "DbOpenValidator.h"

namespace opsqlite {
namespace validators {

bool DbOpenValidator::invalidArgsNumber(std::string &msg, int count) {
  if (count == 0) {
    msg = "[op-sqlite][open] database name is required";
    return true;
  }
  return false;
}

std::string DbOpenValidator::getPath(jsi::Runtime &rt, const jsi::Object &options, const std::string &basePath) {
  std::string path = basePath;
  std::string location = getLocation(rt, options);
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

std::string DbOpenValidator::getLocation(jsi::Runtime &rt, const jsi::Object &options) {
  std::string location = "";
  if (options.hasProperty(rt, "location")) {
    location = options.getProperty(rt, "location").asString(rt).utf8(rt);
  }
  return location;
}

std::string DbOpenValidator::getEncryptionKey(jsi::Runtime &rt, const jsi::Object &options) {
  std::string encryptionKey = "";
  if (options.hasProperty(rt, "encryptionKey")) {
    encryptionKey = options.getProperty(rt, "encryptionKey").asString(rt).utf8(rt);
  }
  return encryptionKey;
}

}
}