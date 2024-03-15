#ifndef utils_h
#define utils_h

#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <jsi/jsilib.h>
#include <map>
#include <stdio.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

jsi::Value toJSI(jsi::Runtime &rt, JSVariant value);

JSVariant toVariant(jsi::Runtime &rt, jsi::Value const &value);

std::vector<JSVariant> toVariantVec(jsi::Runtime &rt, jsi::Value const &args);

jsi::Value createResult(jsi::Runtime &rt, BridgeResult status,
                        std::vector<DumbHostObject> *results,
                        std::shared_ptr<std::vector<SmartHostObject>> metadata);

jsi::Value
create_raw_result(jsi::Runtime &rt, BridgeResult status,
                  const std::vector<std::vector<JSVariant>> *results);

BatchResult importSQLFile(std::string dbName, std::string fileLocation);

int mkdir(const std::string &path);

bool folder_exists(const std::string &foldername);

bool file_exists(const std::string &path);

std::string get_uuid();

std::string sqlite_operation_to_string(int operation_type);

} // namespace opsqlite

#endif /* utils_h */
