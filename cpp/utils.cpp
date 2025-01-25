#include "utils.h"
#include "SmartHostObject.h"
#ifndef OP_SQLITE_USE_LIBSQL
#include "bridge.h"
#endif
#include <fstream>
#include <sys/stat.h>

namespace opsqlite {

namespace jsi = facebook::jsi;

inline jsi::Value to_jsi(jsi::Runtime &rt, const JSVariant &value) {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value);
    } else if (std::holds_alternative<int>(value)) {
        return jsi::Value(std::get<int>(value));
    } else if (std::holds_alternative<long long>(value)) {
        return jsi::Value(static_cast<double>(std::get<long long>(value)));
    } else if (std::holds_alternative<double>(value)) {
        return jsi::Value(std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        auto str = std::get<std::string>(value);
        return jsi::String::createFromUtf8(rt, str);
    } else if (std::holds_alternative<ArrayBuffer>(value)) {
        auto jsBuffer = std::get<ArrayBuffer>(value);
        jsi::Function array_buffer_ctor =
            rt.global().getPropertyAsFunction(rt, "ArrayBuffer");
        jsi::Object o =
            array_buffer_ctor.callAsConstructor(rt, (int)jsBuffer.size)
                .getObject(rt);
        jsi::ArrayBuffer buf = o.getArrayBuffer(rt);
        memcpy(buf.data(rt), jsBuffer.data.get(), jsBuffer.size);
        return o;
    }

    return jsi::Value::null();

    // I wanted to use the visitor pattern here but on the ArrayBuffer case it
    // is somehow throwing a pointer exception Somehow the v.size or
    // v.data.get() is loosing the data when called from the lambda I'm guessing
    // the I created the shared pointer wrong and the memory is being freed
    // before the lambda is called
    //  return std::visit(
    //      [&](auto &&v) -> jsi::Value {
    //        using T = std::decay_t<decltype(v)>;
    //        if constexpr (std::is_same_v<T, bool>) {
    //          return jsi::Value(v);
    //        } else if constexpr (std::is_same_v<T, int>) {
    //          return jsi::Value(v);
    //        } else if constexpr (std::is_same_v<T, long long>) {
    //          return jsi::Value(
    //              static_cast<double>(v)); // JSI doesn't support long long
    //        } else if constexpr (std::is_same_v<T, double>) {
    //          return jsi::Value(v);
    //        } else if constexpr (std::is_same_v<T, std::string>) {
    //          return jsi::String::createFromUtf8(rt, v);
    //        } else if constexpr (std::is_same_v<T, ArrayBuffer>) {
    //          static jsi::Function buffer_constructor =
    //              rt.global().getPropertyAsFunction(rt, "ArrayBuffer");
    //          jsi::Object o =
    //              buffer_constructor.callAsConstructor(rt,
    //              static_cast<int>(v.size))
    //                  .getObject(rt);
    //          jsi::ArrayBuffer buf = o.getArrayBuffer(rt);
    //          memcpy(buf.data(rt), v.data.get(), v.size);
    //          return o;
    //        } else {
    //          return jsi::Value::null();
    //        }
    //      },
    //      value);
}

inline JSVariant to_variant(jsi::Runtime &rt, const jsi::Value &value) {
    if (value.isNull() || value.isUndefined()) {
        return JSVariant(nullptr);
    } else if (value.isBool()) {
        return JSVariant(value.getBool());
    } else if (value.isNumber()) {
        double doubleVal = value.asNumber();
        int intVal = (int)doubleVal;
        long long longVal = (long)doubleVal;
        if (intVal == doubleVal) {
            return JSVariant(intVal);
        } else if (longVal == doubleVal) {
            return JSVariant(longVal);
        } else {
            return JSVariant(doubleVal);
        }
    } else if (value.isString()) {
        std::string strVal = value.asString(rt).utf8(rt);
        return JSVariant(strVal);
    } else if (value.isObject()) {
        auto obj = value.asObject(rt);

        if (!obj.isArrayBuffer(rt)) {
            throw std::runtime_error(
                "Object is not an ArrayBuffer, cannot bind to SQLite");
        }

        auto buffer = obj.getArrayBuffer(rt);
        uint8_t *data = new uint8_t[buffer.size(rt)];
        memcpy(data, buffer.data(rt), buffer.size(rt));

        return JSVariant(ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                     .size = buffer.size(rt)});
    }

    throw std::runtime_error("Cannot convert JSI value to C++ Variant value");
}

std::vector<std::string> to_string_vec(jsi::Runtime &rt, jsi::Value const &xs) {
    jsi::Array values = xs.asObject(rt).asArray(rt);
    std::vector<std::string> res;
    for (int ii = 0; ii < values.length(rt); ii++) {
        std::string value =
            values.getValueAtIndex(rt, ii).asString(rt).utf8(rt);
        res.emplace_back(value);
    }
    return res;
}

std::vector<int> to_int_vec(jsi::Runtime &rt, jsi::Value const &xs) {
    jsi::Array values = xs.asObject(rt).asArray(rt);
    std::vector<int> res;
    for (int ii = 0; ii < values.length(rt); ii++) {
        int value = static_cast<int>(values.getValueAtIndex(rt, ii).asNumber());
        res.emplace_back(value);
    }
    return res;
}

std::vector<JSVariant> to_variant_vec(jsi::Runtime &rt, jsi::Value const &xs) {
    std::vector<JSVariant> res;
    jsi::Array values = xs.asObject(rt).asArray(rt);

    for (int ii = 0; ii < values.length(rt); ii++) {
        jsi::Value value = values.getValueAtIndex(rt, ii);
        res.emplace_back(to_variant(rt, value));
    }

    return res;
}

jsi::Value create_js_rows(jsi::Runtime &rt, const BridgeResult &status) {
    jsi::Object res = jsi::Object(rt);

    res.setProperty(rt, "rowsAffected", status.affectedRows);
    if (status.affectedRows > 0 && status.insertId != 0) {
        res.setProperty(rt, "insertId", jsi::Value(status.insertId));
    }

    size_t row_count = status.rows.size();
    auto rows = jsi::Array(rt, row_count);

    if (row_count > 0) {
        for (int i = 0; i < row_count; i++) {
            auto row = jsi::Array(rt, status.column_names.size());
            std::vector<JSVariant> native_row = status.rows[i];
            for (int j = 0; j < native_row.size(); j++) {
                auto value = to_jsi(rt, native_row[j]);
                row.setValueAtIndex(rt, j, value);
            }
            rows.setValueAtIndex(rt, i, row);
        }
    }
    res.setProperty(rt, "rawRows", rows);

    size_t column_count = status.column_names.size();
    auto column_array = jsi::Array(rt, column_count);
    for (int i = 0; i < column_count; i++) {
        auto column = status.column_names.at(i);
        column_array.setValueAtIndex(rt, i, to_jsi(rt, column));
    }
    res.setProperty(rt, "columnNames", std::move(column_array));
    return res;
}

jsi::Value
create_result(jsi::Runtime &rt, const BridgeResult &status,
              std::vector<DumbHostObject> *results,
              std::shared_ptr<std::vector<SmartHostObject>> metadata) {
    jsi::Object res = jsi::Object(rt);

    res.setProperty(rt, "rowsAffected", status.affectedRows);
    if (status.affectedRows > 0 && status.insertId != 0) {
        res.setProperty(rt, "insertId", jsi::Value(status.insertId));
    }

    size_t rowCount = results->size();

    auto array = jsi::Array(rt, rowCount);
    for (int i = 0; i < rowCount; i++) {
        auto obj = results->at(i);
        array.setValueAtIndex(rt, i,
                              jsi::Object::createFromHostObject(
                                  rt, std::make_shared<DumbHostObject>(obj)));
    }
    res.setProperty(rt, "rows", std::move(array));

    size_t column_count = metadata->size();
    auto column_array = jsi::Array(rt, column_count);
    for (int i = 0; i < column_count; i++) {
        auto column = metadata->at(i);
        column_array.setValueAtIndex(
            rt, i,
            jsi::Object::createFromHostObject(
                rt, std::make_shared<SmartHostObject>(column)));
    }
    res.setProperty(rt, "metadata", std::move(column_array));

    return std::move(res);
}

jsi::Value
create_raw_result(jsi::Runtime &rt, const BridgeResult &status,
                  const std::vector<std::vector<JSVariant>> *results) {
    size_t row_count = results->size();
    jsi::Array res = jsi::Array(rt, row_count);
    for (int i = 0; i < row_count; i++) {
        auto row = results->at(i);
        auto array = jsi::Array(rt, row.size());
        for (int j = 0; j < row.size(); j++) {
            array.setValueAtIndex(rt, j, to_jsi(rt, row[j]));
        }
        res.setValueAtIndex(rt, i, array);
    }
    return res;
}

void to_batch_arguments(jsi::Runtime &rt, jsi::Array const &tuples,
                        std::vector<BatchArguments> *commands) {
    for (int i = 0; i < tuples.length(rt); i++) {
        const jsi::Array &tuple =
            tuples.getValueAtIndex(rt, i).asObject(rt).asArray(rt);
        const size_t length = tuple.length(rt);
        if (length == 0) {
            continue;
        }

        const std::string query =
            tuple.getValueAtIndex(rt, 0).asString(rt).utf8(rt);
        if (length == 1) {
            commands->push_back({query});
            continue;
        }

        const jsi::Value &tuple_params = tuple.getValueAtIndex(rt, 1);

        if (!tuple_params.isUndefined() &&
            tuple_params.asObject(rt).isArray(rt) &&
            tuple_params.asObject(rt).asArray(rt).length(rt) > 0 &&
            tuple_params.asObject(rt)
                .asArray(rt)
                .getValueAtIndex(rt, 0)
                .isObject()) {
            // The params for this tuple is an array itself
            // The query should repeat for each element in the array
            const jsi::Array &params_array =
                tuple_params.asObject(rt).asArray(rt);
            for (int x = 0; x < params_array.length(rt); x++) {
                const jsi::Value &p = params_array.getValueAtIndex(rt, x);
                auto params = std::vector<JSVariant>(to_variant_vec(rt, p));
                commands->push_back({query, params});
            }
        } else {
            auto params =
                std::vector<JSVariant>(to_variant_vec(rt, tuple_params));
            commands->push_back({query, params});
        }
    }
}

#ifndef OP_SQLITE_USE_LIBSQL
BatchResult import_sql_file(sqlite3 *db, std::string path) {
    std::string line;
    std::ifstream sqFile(path);
    if (!sqFile.is_open()) {
        throw std::runtime_error("Could not open file: " + path);
    }

    try {
        int affectedRows = 0;
        int commands = 0;
        opsqlite_execute(db, "BEGIN EXCLUSIVE TRANSACTION", nullptr);
        while (std::getline(sqFile, line, '\n')) {
            if (!line.empty()) {
                try {
                    auto result = opsqlite_execute(db, line, nullptr);
                    affectedRows += result.affectedRows;
                    commands++;
                } catch (std::exception &exc) {
                    opsqlite_execute(db, "ROLLBACK", nullptr);
                    sqFile.close();
                    throw exc;
                }
            }
        }
        sqFile.close();
        opsqlite_execute(db, "COMMIT", nullptr);
        return {"", affectedRows, commands};
    } catch (std::exception &exc) {
        sqFile.close();
        opsqlite_execute(db, "ROLLBACK", nullptr);
        throw exc;
    }
}
#endif

bool folder_exists(const std::string &name) {
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

bool file_exists(const std::string &path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

void log_to_console(jsi::Runtime &runtime, const std::string &message) {
    auto console = runtime.global().getPropertyAsObject(runtime, "console");
    auto log = console.getPropertyAsFunction(runtime, "log");
    log.call(runtime, jsi::String::createFromUtf8(runtime, message));
}

} // namespace opsqlite
