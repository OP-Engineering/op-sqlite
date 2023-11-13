#include "utils.h"
#include "DynamicHostObject.h"
#include <iostream>
#include <fstream>
#include "bridge.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

jsi::Value toJSI(jsi::Runtime &rt, JSVariant value) {
    
    if (std::holds_alternative<bool>(value))
    {
        return std::get<bool>(value);
    }
    else if (std::holds_alternative<int>(value))
    {
        return jsi::Value(std::get<int>(value));
    }
    else if (std::holds_alternative<long long>(value))
    {
        return jsi::Value(static_cast<double>(std::get<long long>(value)));
    }
    else if (std::holds_alternative<double>(value))
    {
        return jsi::Value(std::get<double>(value));
    }
    else if (std::holds_alternative<std::string>(value))
    {
        return jsi::String::createFromUtf8(rt, std::get<std::string>(value));
    }
    else if (std::holds_alternative<ArrayBuffer>(value))
    {
        auto jsBuffer = std::get<ArrayBuffer>(value);
        jsi::Function array_buffer_ctor = rt.global().getPropertyAsFunction(rt, "ArrayBuffer");
        jsi::Object o = array_buffer_ctor.callAsConstructor(rt, (int)jsBuffer.size).getObject(rt);
        jsi::ArrayBuffer buf = o.getArrayBuffer(rt);
        // You cannot share raw memory between native and JS
        // always copy the data
        // see https://github.com/facebook/hermes/pull/419 and https://github.com/facebook/hermes/issues/564.
        memcpy(buf.data(rt), jsBuffer.data.get(), jsBuffer.size);
        return o;
    }
    
    return jsi::Value::null();
}

std::vector<JSVariant> toVariantVec(jsi::Runtime &rt, jsi::Value const &params)
{
    std::vector<JSVariant> res;
    
    if (params.isNull() || params.isUndefined())
    {
        return res;
    }
    
    jsi::Array values = params.asObject(rt).asArray(rt);
    
    for (int ii = 0; ii < values.length(rt); ii++)
    {
        jsi::Value value = values.getValueAtIndex(rt, ii);
        
        if (value.isNull() || value.isUndefined())
        {
            res.push_back(JSVariant(nullptr));
        }
        else if (value.isBool())
        {
            res.push_back(JSVariant(value.getBool()));
        }
        else if (value.isNumber())
        {
            double doubleVal = value.asNumber();
            int intVal = (int)doubleVal;
            long long longVal = (long)doubleVal;
            if (intVal == doubleVal)
            {
                res.push_back(JSVariant(intVal));
            }
            else if (longVal == doubleVal)
            {
                res.push_back(JSVariant(longVal));
            }
            else
            {
                res.push_back(JSVariant(doubleVal));
            }
        }
        else if (value.isString())
        {
            std::string strVal = value.asString(rt).utf8(rt);
            res.push_back(JSVariant(strVal));
        }
        else if (value.isObject())
        {
            auto obj = value.asObject(rt);
            if (obj.isArrayBuffer(rt)) {
                auto buffer = obj.getArrayBuffer(rt);
                
                uint8_t *data = new uint8_t[buffer.size(rt)];
                // You cannot share raw memory between native and JS
                // always copy the data
                // see https://github.com/facebook/hermes/pull/419 and https://github.com/facebook/hermes/issues/564.
                memcpy(data, buffer.data(rt), buffer.size(rt));
                
                res.push_back(JSVariant(ArrayBuffer {
                    .data =  std::shared_ptr<uint8_t>{data},
                    .size =  buffer.size(rt)
                }));
            } else {
                throw std::invalid_argument("Unknown JSI ArrayBuffer to variant value conversion, received object instead of ArrayBuffer");
            }
        } else {
            throw std::invalid_argument("Unknown JSI to variant value conversion");
        }
    }
    
    return res;
}

jsi::Value createResult(jsi::Runtime &rt,
                        BridgeResult status,
                        std::vector<DumbHostObject> *results,
                        std::shared_ptr<std::vector<DynamicHostObject>> metadata)
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
            auto obj = results->at(i);
            array.setValueAtIndex(rt, i, jsi::Object::createFromHostObject(rt, std::make_shared<DumbHostObject>(obj)));
        }
        rows.setProperty(rt, "_array", std::move(array));
        res.setProperty(rt, "rows", std::move(rows));
    }
    
    size_t column_count = metadata->size();
    auto column_array = jsi::Array(rt, column_count);
    for (int i = 0; i < column_count; i++) {
        auto column = metadata->at(i);
        column_array.setValueAtIndex(rt, i, jsi::Object::createFromHostObject(rt, std::make_shared<DynamicHostObject>(column)));
    }
    res.setProperty(rt, "metadata", std::move(column_array));
    
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
