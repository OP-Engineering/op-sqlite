#ifndef utils_h
#define utils_h

#include <stdio.h>
#include <jsi/jsilib.h>
#include <jsi/jsi.h>
#include <vector>
#include <map>
#include <any>
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

// TODO get rid of this
struct SequelBatchOperationResult
{
    ResultType type;
    std::string message;
    int affectedRows;
    int commands;
};

/**
 * Describe column information of a resultset
 */
struct QuickColumnMetadata
{
    std::string colunmName;
    int columnIndex;
    std::string columnDeclaredType;
};


std::any toAny(jsi::Runtime &rt, jsi::Value &value);

jsi::Value toJSI(jsi::Runtime &rt, std::any value);


std::vector<std::any> jsiQueryArgumentsToSequelParam(jsi::Runtime &rt, jsi::Value const &args);

jsi::Value createResult(jsi::Runtime &rt,
                        BridgeResult status,
                        std::vector<std::shared_ptr<DynamicHostObject>> *results,
                        std::vector<QuickColumnMetadata> *metadata);

SequelBatchOperationResult importSQLFile(std::string dbName, std::string fileLocation);

}

#endif /* utils_h */
