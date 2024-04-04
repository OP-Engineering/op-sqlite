//
// Created by jplc on 4/4/24.
//

#ifndef OPSQLITEEXAMPLE_VALIDATORS_DBATTACHVALIDATOR_H
#define OPSQLITEEXAMPLE_VALIDATORS_DBATTACHVALIDATOR_H

#include <jsi/jsi.h>
#include <string>

namespace jsi = facebook::jsi;

namespace opsqlite {
namespace validators {

class DbAttachValidator {
public:
  static bool invalidArgsNumber(std::string &msg, int count);
  static bool noStringArgs(std::string &msg, const jsi::Value &arg1, const jsi::Value &arg2, const jsi::Value &arg3);
  static bool locationArgDefined(const int count, const jsi::Value &arg);
  static bool locationArgIsNotString(std::string &msg, const jsi::Value &arg);

private:
  DbAttachValidator();
};

}
}

#endif //OPSQLITEEXAMPLE_VALIDATORS_DBATTACHVALIDATOR_H
