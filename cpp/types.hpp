#pragma once

#include <ReactCommon/CallInvoker.h>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <variant>
#include <vector>

namespace opsqlite {

extern std::shared_ptr<facebook::react::CallInvoker> invoker;
extern bool invalidated;

struct ArrayBuffer {
  std::shared_ptr<uint8_t> data;
  size_t size;
};

using JSVariant = std::variant<nullptr_t, bool, int, double, long, long long,
                               std::string, ArrayBuffer>;

struct BridgeResult {
  std::string message;
  int affectedRows;
  double insertId;
  std::vector<std::vector<JSVariant>> rows;
  std::vector<std::string> column_names;
};

struct BatchResult {
  std::string message;
  int affectedRows;
  int commands;
};

struct BatchArguments {
  std::string sql;
  std::vector<JSVariant> params;
};

} // namespace opsqlite
