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

jsi::Value toJSI(jsi::Runtime &rt, const JSVariant &value);
JSVariant toVariant(jsi::Runtime &rt, jsi::Value const &value);
std::vector<std::string> to_string_vec(jsi::Runtime &rt, jsi::Value const &xs);
std::vector<JSVariant> to_variant_vec(jsi::Runtime &rt, jsi::Value const &xs);
std::vector<int> to_int_vec(jsi::Runtime &rt, jsi::Value const &xs);
jsi::Value createResult(jsi::Runtime &rt, BridgeResult status,
                        std::vector<DumbHostObject> *results,
                        std::shared_ptr<std::vector<SmartHostObject>> metadata);
jsi::Value create_js_rows(jsi::Runtime &rt, const BridgeResult &status);
jsi::Value
create_raw_result(jsi::Runtime &rt, BridgeResult status,
                  const std::vector<std::vector<JSVariant>> *results);

void to_batch_arguments(jsi::Runtime &rt, jsi::Array const &batchParams,
                        std::vector<BatchArguments> *commands);

BatchResult importSQLFile(std::string dbName, std::string fileLocation);

int mkdir(const std::string &path);

bool folder_exists(const std::string &foldername);

bool file_exists(const std::string &path);

} // namespace opsqlite

#endif /* utils_h */
