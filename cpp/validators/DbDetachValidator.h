//
// Created by jplc on 4/4/24.
//

#ifndef OPSQLITEEXAMPLE_VALIDATORS_DBDETACHVALIDATOR_H
#define OPSQLITEEXAMPLE_VALIDATORS_DBDETACHVALIDATOR_H

#include <jsi/jsi.h>
#include <string>

namespace jsi = facebook::jsi;

namespace opsqlite {
namespace validators {

class DbDetachValidator {
public:
  static bool invalidArgsNumber(std::string &msg, int count);
  static bool noStringArgs(std::string &msg, const jsi::Value &arg0, const jsi::Value &arg1);

private:
  DbDetachValidator();
};

}
}

#endif //OPSQLITEEXAMPLE_VALIDATORS_DBDETACHVALIDATOR_H
