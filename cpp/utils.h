#ifndef utils_h
#define utils_h

#include <stdio.h>
#include <jsi/jsilib.h>
#include <jsi/jsi.h>
#include <vector>
#include <map>
#include <any>
#include "types.h"
#include "DynamicHostObject.h"

namespace osp {

namespace jsi = facebook::jsi;



enum ResultType
{
    SQLiteOk,
    SQLiteError
};

struct BridgeResult
{
    ResultType type;
    std::string message;
    int affectedRows;
    double insertId;
};

struct BatchResult
{
    ResultType type;
    std::string message;
    int affectedRows;
    int commands;
};

jsVal toAny(jsi::Runtime &rt, jsi::Value &value);

jsi::Value toJSI(jsi::Runtime &rt, jsVal value);

std::vector<jsVal> toAnyVec(jsi::Runtime &rt, jsi::Value const &args);

jsi::Value createResult(jsi::Runtime &rt,
                        BridgeResult status,
                        std::vector<std::shared_ptr<DynamicHostObject>> *results,
                        std::vector<std::shared_ptr<DynamicHostObject>> *metadata);

BatchResult importSQLFile(std::string dbName, std::string fileLocation);

}

#endif /* utils_h */
