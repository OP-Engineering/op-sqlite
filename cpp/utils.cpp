#include "utils.h"
#include "DynamicHostObject.h"
#include <iostream>
#include <fstream>
#include "bridge.h"

namespace osp {

namespace jsi = facebook::jsi;

std::any toAny(jsi::Runtime &rt, jsi::Value &value) {
    if (value.isNull() || value.isUndefined())
    {
        return std::any(nullptr);
    }
    else if (value.isBool())
    {
        return std::any(value.getBool());
    }
    else if (value.isNumber())
    {
        double doubleVal = value.asNumber();
        int intVal = (int)doubleVal;
        long long longVal = (long)doubleVal;
        if (intVal == doubleVal)
        {
            return std::any(intVal);
        }
        else if (longVal == doubleVal)
        {
            return std::any(longVal);
        }
        else
        {
            return std::any(doubleVal);
        }
    }
    else if (value.isString())
    {
        std::string strVal = value.asString(rt).utf8(rt);
        return std::any(strVal);
    }
    //    else if (value.isObject())
    //    {
    //        auto obj = value.asObject(rt);
    //        if (obj.isArrayBuffer(rt))
    //        {
    //            auto buf = obj.getArrayBuffer(rt);
    //            target->push_back(createArrayBufferQuickValue(buf.data(rt), buf.size(rt)));
    //        }
    //    }
    //    else
    //    {
    //        target->push_back(createNullQuickValue());
    //    }
    
    throw new std::invalid_argument("Unknown JSI to any value conversion");
}

jsi::Value toJSI(jsi::Runtime &rt, std::any value) {
    const std::type_info &type(value.type());
    
    if (type == typeid(NULL) || type == typeid(nullptr))
    {
        return jsi::Value::null();
    }
    else if (type == typeid(bool))
    {
        return std::any_cast<bool>(value);
    }
    else if (type == typeid(int))
    {
        return jsi::Value(std::any_cast<int>(value));
    }
    else if (type == typeid(long long))
    {
        return jsi::Value(static_cast<double>(std::any_cast<long long>(value)));
    }
    else if (type == typeid(double))
    {
        return jsi::Value(std::any_cast<double>(value));
    }
    else if (type == typeid(std::string))
    {
        return jsi::String::createFromUtf8(rt, std::any_cast<std::string>(value));
    }
    else if (type == typeid(const char*))
    {
        return jsi::String::createFromAscii(rt, std::any_cast<const char*>(value));
    }
    // TODO Add support for array buffers
    //        else if (value.isObject())
    //        {
    //            auto obj = value.asObject(rt);
    //            if(obj.isArrayBuffer(rt)) {
    //                auto buf = obj.getArrayBuffer(rt);
    //                sqlite3_bind_blob(statement, sqIndex, buf.data(rt), buf.size(rt), SQLITE_STATIC);
    //            }
    //
    //        }
    
    throw std::invalid_argument("Unsupported scalar type, cannot convert to JSI Value");
}

std::vector<std::any> jsiQueryArgumentsToSequelParam(jsi::Runtime &rt, jsi::Value const &params)
{
    
    std::vector<std::any> res;
    
    if (params.isNull() || params.isUndefined())
    {
        return res;
    }
    
    jsi::Array values = params.asObject(rt).asArray(rt);
    
    for (int ii = 0; ii < values.length(rt); ii++)
    {
        
        jsi::Value value = values.getValueAtIndex(rt, ii);
        res.push_back(toAny(rt, value));
    }
    
    return res;
}

jsi::Value createResult(jsi::Runtime &rt,
                        BridgeResult status,
                        std::vector<std::shared_ptr<DynamicHostObject>> *results,
                        std::vector<std::shared_ptr<DynamicHostObject>> *metadata)
{
    if(status.type == SQLiteError) {
        throw std::invalid_argument(status.message);
    }
    
    jsi::Object res = jsi::Object(rt);
    
    res.setProperty(rt, "rowsAffected", status.affectedRows);
    if (status.affectedRows > 0 && status.insertId != 0)
    {
        res.setProperty(rt, "insertId", jsi::Value(status.insertId));
    }
    
    size_t rowCount = results->size();
    jsi::Object rows = jsi::Object(rt);
    rows.setProperty(rt, "length", jsi::Value((int)rowCount));
    
    if (rowCount > 0)
    {
        auto array = jsi::Array(rt, rowCount);
        for (int i = 0; i < rowCount; i++)
        {
            array.setValueAtIndex(rt, i, jsi::Object::createFromHostObject(rt, results->at(i)));
        }
        rows.setProperty(rt, "_array", std::move(array));
        res.setProperty(rt, "rows", std::move(rows));
    }
    
    if(metadata != nullptr)
    {
        size_t column_count = metadata->size();
        auto column_array = jsi::Array(rt, column_count);
        for (int i = 0; i < column_count; i++) {
            auto column = metadata->at(i);
            column_array.setValueAtIndex(rt, i, jsi::Object::createFromHostObject(rt, column));
        }
        res.setProperty(rt, "metadata", std::move(column_array));
    }
    
    return std::move(res);
}

BatchResult importSQLFile(std::string dbName, std::string fileLocation)
{
    std::string line;
    std::ifstream sqFile(fileLocation);
    if (sqFile.is_open())
    {
        try
        {
            int affectedRows = 0;
            int commands = 0;
            sqliteExecuteLiteral(dbName, "BEGIN EXCLUSIVE TRANSACTION");
            while (std::getline(sqFile, line, '\n'))
            {
                if (!line.empty())
                {
                    BridgeResult result = sqliteExecuteLiteral(dbName, line);
                    if (result.type == SQLiteError)
                    {
                        sqliteExecuteLiteral(dbName, "ROLLBACK");
                        sqFile.close();
                        return {SQLiteError, result.message, 0, commands};
                    }
                    else
                    {
                        affectedRows += result.affectedRows;
                        commands++;
                    }
                }
            }
            sqFile.close();
            sqliteExecuteLiteral(dbName, "COMMIT");
            return {SQLiteOk, "", affectedRows, commands};
        }
        catch (...)
        {
            sqFile.close();
            sqliteExecuteLiteral(dbName, "ROLLBACK");
            return {SQLiteError, "[op-sqlite][loadSQLFile] Unexpected error, transaction was rolledback", 0, 0};
        }
    }
    else
    {
        return {SQLiteError, "[op-sqlite][loadSQLFile] Could not open file", 0, 0};
    }
}


}
