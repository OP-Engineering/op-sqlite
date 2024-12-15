#include "utils.h"
#include "SmartHostObject.h"
#ifndef OP_SQLITE_USE_LIBSQL
#include "bridge.h"
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace opsqlite {

namespace jsi = facebook::jsi;

jsi::Value toJSI(jsi::Runtime &rt, const JSVariant &value) {
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
    jsi::Object o = array_buffer_ctor.callAsConstructor(rt, (int)jsBuffer.size)
                        .getObject(rt);
    jsi::ArrayBuffer buf = o.getArrayBuffer(rt);
    memcpy(buf.data(rt), jsBuffer.data.get(), jsBuffer.size);
    return o;
  }

  return jsi::Value::null();
}

JSVariant toVariant(jsi::Runtime &rt, const jsi::Value &value) {
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
      throw std::invalid_argument(
          "Object is not an ArrayBuffer, cannot bind to SQLite");
    }

    auto buffer = obj.getArrayBuffer(rt);
    uint8_t *data = new uint8_t[buffer.size(rt)];
    memcpy(data, buffer.data(rt), buffer.size(rt));

    return JSVariant(ArrayBuffer{.data = std::shared_ptr<uint8_t>{data},
                                 .size = buffer.size(rt)});

  } else {
    throw std::invalid_argument(
        "Cannot convert JSI value to C++ Variant value");
  }
}

std::vector<std::string> to_string_vec(jsi::Runtime &rt, jsi::Value const &xs) {
  jsi::Array values = xs.asObject(rt).asArray(rt);
  std::vector<std::string> res;
  for (int ii = 0; ii < values.length(rt); ii++) {
    std::string value = values.getValueAtIndex(rt, ii).asString(rt).utf8(rt);
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

  if (xs.isNull() || xs.isUndefined()) {
    return res;
  }

  jsi::Array values = xs.asObject(rt).asArray(rt);

  for (int ii = 0; ii < values.length(rt); ii++) {
    jsi::Value value = values.getValueAtIndex(rt, ii);
    res.emplace_back(toVariant(rt, value));
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
        auto value = toJSI(rt, native_row[j]);
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
    column_array.setValueAtIndex(rt, i, toJSI(rt, column));
  }
  res.setProperty(rt, "columnNames", std::move(column_array));
  return res;
}

jsi::Value
create_result(jsi::Runtime &rt, BridgeResult status,
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
create_raw_result(jsi::Runtime &rt, BridgeResult status,
                  const std::vector<std::vector<JSVariant>> *results) {
  size_t row_count = results->size();
  jsi::Array res = jsi::Array(rt, row_count);
  for (int i = 0; i < row_count; i++) {
    auto row = results->at(i);
    auto array = jsi::Array(rt, row.size());
    for (int j = 0; j < row.size(); j++) {
      array.setValueAtIndex(rt, j, toJSI(rt, row[j]));
    }
    res.setValueAtIndex(rt, i, array);
  }
  return res;
}

void to_batch_arguments(jsi::Runtime &rt, jsi::Array const &batchParams,
                        std::vector<BatchArguments> *commands) {
  for (int i = 0; i < batchParams.length(rt); i++) {
    const jsi::Array &command =
        batchParams.getValueAtIndex(rt, i).asObject(rt).asArray(rt);
    if (command.length(rt) == 0) {
      continue;
    }

    const std::string query =
        command.getValueAtIndex(rt, 0).asString(rt).utf8(rt);
    const jsi::Value &commandParams = command.length(rt) > 1
                                          ? command.getValueAtIndex(rt, 1)
                                          : jsi::Value::undefined();
    if (!commandParams.isUndefined() &&
        commandParams.asObject(rt).isArray(rt) &&
        commandParams.asObject(rt).asArray(rt).length(rt) > 0 &&
        commandParams.asObject(rt)
            .asArray(rt)
            .getValueAtIndex(rt, 0)
            .isObject()) {
      // This arguments is an array of arrays, like a batch update of a single
      // sql command.
      const jsi::Array &batchUpdateParams =
          commandParams.asObject(rt).asArray(rt);
      for (int x = 0; x < batchUpdateParams.length(rt); x++) {
        const jsi::Value &p = batchUpdateParams.getValueAtIndex(rt, x);
        auto params =
            std::make_shared<std::vector<JSVariant>>(to_variant_vec(rt, p));
        commands->push_back({query, params});
      }
    } else {
      auto params = std::make_shared<std::vector<JSVariant>>(
          to_variant_vec(rt, commandParams));
      commands->push_back({query, params});
    }
  }
}

#ifndef OP_SQLITE_USE_LIBSQL
BatchResult import_sql_file(sqlite3 *db, std::string fileLocation) {
  std::string line;
  std::ifstream sqFile(fileLocation);
  if (!sqFile.is_open()) {
    throw std::runtime_error("Could not open file: " + fileLocation);
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

bool folder_exists(const std::string &foldername) {
  struct stat buffer;
  return (stat(foldername.c_str(), &buffer) == 0);
}

bool file_exists(const std::string &path) {
  struct stat buffer;
  return (stat(path.c_str(), &buffer) == 0);
}

int mkdir(std::string const &path) {
  std::filesystem::create_directories(path);
  return 0;
}

std::vector<std::string> parse_string_list(const std::string &str) {
  std::vector<std::string> result;
  std::istringstream stream(str);
  std::string token;
  while (std::getline(stream, token, ',')) {
    result.emplace_back(token);
  }
  return result;
}

} // namespace opsqlite
