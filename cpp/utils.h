#ifndef utils_h
#define utils_h

#include "DumbHostObject.h"
#include "DynamicHostObject.h"
#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <jsi/jsilib.h>
#include <map>
#include <stdio.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

enum ResultType { SQLiteOk, SQLiteError };

struct BridgeResult {
  ResultType type;
  std::string message;
  int affectedRows;
  double insertId;
};

struct BatchResult {
  ResultType type;
  std::string message;
  int affectedRows;
  int commands;
};

jsi::Value toJSI(jsi::Runtime &rt, JSVariant value);

std::vector<JSVariant> toVariantVec(jsi::Runtime &rt, jsi::Value const &args);

jsi::Value
createResult(jsi::Runtime &rt, BridgeResult status,
             std::vector<DumbHostObject> *results,
             std::shared_ptr<std::vector<DynamicHostObject>> metadata);

BatchResult importSQLFile(std::string dbName, std::string fileLocation);

} // namespace opsqlite

#endif /* utils_h */
