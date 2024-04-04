#ifndef OPSQLITEEXAMPLE_VALIDATORS_DBOPENVALIDATOR_H
#define OPSQLITEEXAMPLE_VALIDATORS_DBOPENVALIDATOR_H

#include <jsi/jsi.h>
#include <string>

namespace jsi = facebook::jsi;

namespace opsqlite {
namespace validators {

class DbOpenValidator {
public:
  static bool isParametersNumberNotOk(std::string &msg, int count);
  static std::string getPath(jsi::Runtime &rt, const jsi::Object &options, const std::string &basePath);
  static std::string getLocation(jsi::Runtime &rt, const jsi::Object &options);
  static std::string getEncryptionKey(jsi::Runtime &rt, const jsi::Object &options);

private:
  DbOpenValidator();
};

}
}

#endif // OPSQLITEEXAMPLE_VALIDATORS_DBOPENVALIDATOR_H