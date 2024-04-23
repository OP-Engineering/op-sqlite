#include "utils.h"
#include "SmartHostObject.h"
#include "bridge.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace opsqlite {

namespace jsi = facebook::jsi;

jsi::Value toJSI(jsi::Runtime &rt, JSVariant value) {

  if (std::holds_alternative<bool>(value)) {
    return std::get<bool>(value);
  } else if (std::holds_alternative<int>(value)) {
    return jsi::Value(std::get<int>(value));
  } else if (std::holds_alternative<long long>(value)) {
    return jsi::Value(static_cast<double>(std::get<long long>(value)));
  } else if (std::holds_alternative<double>(value)) {
    return jsi::Value(std::get<double>(value));
  } else if (std::holds_alternative<std::string>(value)) {
    return jsi::String::createFromUtf8(rt, std::get<std::string>(value));
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
          "Objects returned by OP-SQLite, are C++ HostObjects and thus cannot "
          "store any object, only scalar "
          "properties (int, long, double, string, bool) and ArrayBuffers.");
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

std::vector<JSVariant> toVariantVec(jsi::Runtime &rt,
                                    jsi::Value const &params) {
  std::vector<JSVariant> res;

  if (params.isNull() || params.isUndefined()) {
    return res;
  }

  jsi::Array values = params.asObject(rt).asArray(rt);

  for (int ii = 0; ii < values.length(rt); ii++) {
    jsi::Value value = values.getValueAtIndex(rt, ii);

    if (value.isNull() || value.isUndefined()) {
      res.push_back(JSVariant(nullptr));
    } else if (value.isBool()) {
      res.push_back(JSVariant(value.getBool()));
    } else if (value.isNumber()) {
      double doubleVal = value.asNumber();
      int intVal = (int)doubleVal;
      long long longVal = (long)doubleVal;
      if (intVal == doubleVal) {
        res.push_back(JSVariant(intVal));
      } else if (longVal == doubleVal) {
        res.push_back(JSVariant(longVal));
      } else {
        res.push_back(JSVariant(doubleVal));
      }
    } else if (value.isString()) {
      std::string strVal = value.asString(rt).utf8(rt);
      res.push_back(JSVariant(strVal));
    } else if (value.isObject()) {
      auto obj = value.asObject(rt);
      if (obj.isArrayBuffer(rt)) {
        auto buffer = obj.getArrayBuffer(rt);
        size_t size = buffer.size(rt);
        uint8_t *data = new uint8_t[size];
        // You cannot share raw memory between native and JS
        // always copy the data
        // see https://github.com/facebook/hermes/pull/419 and
        // https://github.com/facebook/hermes/issues/564.
        memcpy(data, buffer.data(rt), size);

        res.push_back(JSVariant(ArrayBuffer{
            .data = std::shared_ptr<uint8_t>{data}, .size = buffer.size(rt)}));
      } else {
        throw std::invalid_argument(
            "Unknown JSI ArrayBuffer to variant value conversion, received "
            "object instead of ArrayBuffer");
      }
    } else {
      throw std::invalid_argument("Unknown JSI to variant value conversion");
    }
  }

  return res;
}

jsi::Value
createResult(jsi::Runtime &rt, BridgeResult status,
             std::vector<DumbHostObject> *results,
             std::shared_ptr<std::vector<SmartHostObject>> metadata) {
  if (status.type == SQLiteError) {
    throw std::invalid_argument(status.message);
  }

  jsi::Object res = jsi::Object(rt);

  res.setProperty(rt, "rowsAffected", status.affectedRows);
  if (status.affectedRows > 0 && status.insertId != 0) {
    res.setProperty(rt, "insertId", jsi::Value(status.insertId));
  }

  size_t rowCount = results->size();
  jsi::Object rows = jsi::Object(rt);
  rows.setProperty(rt, "length", jsi::Value((int)rowCount));

  if (rowCount > 0) {
    auto array = jsi::Array(rt, rowCount);
    for (int i = 0; i < rowCount; i++) {
      auto obj = results->at(i);
      array.setValueAtIndex(rt, i,
                            jsi::Object::createFromHostObject(
                                rt, std::make_shared<DumbHostObject>(obj)));
    }
    rows.setProperty(rt, "_array", std::move(array));
    res.setProperty(rt, "rows", std::move(rows));
  }

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

BatchResult importSQLFile(std::string dbName, std::string fileLocation) {
  std::string line;
  std::ifstream sqFile(fileLocation);
  if (sqFile.is_open()) {
    try {
      int affectedRows = 0;
      int commands = 0;
      opsqlite_execute(dbName, "BEGIN EXCLUSIVE TRANSACTION", nullptr, nullptr,
                       nullptr);
      while (std::getline(sqFile, line, '\n')) {
        if (!line.empty()) {
          BridgeResult result =
              opsqlite_execute(dbName, line, nullptr, nullptr, nullptr);
          if (result.type == SQLiteError) {
            opsqlite_execute(dbName, "ROLLBACK", nullptr, nullptr, nullptr);
            sqFile.close();
            return {SQLiteError, result.message, 0, commands};
          } else {
            affectedRows += result.affectedRows;
            commands++;
          }
        }
      }
      sqFile.close();
      opsqlite_execute(dbName, "COMMIT", nullptr, nullptr, nullptr);
      return {SQLiteOk, "", affectedRows, commands};
    } catch (...) {
      sqFile.close();
      opsqlite_execute(dbName, "ROLLBACK", nullptr, nullptr, nullptr);
      return {SQLiteError,
              "[op-sqlite][loadSQLFile] Unexpected error, transaction was "
              "rolledback",
              0, 0};
    }
  } else {
    return {SQLiteError, "[op-sqlite][loadSQLFile] Could not open file", 0, 0};
  }
}

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

} // namespace opsqlite
