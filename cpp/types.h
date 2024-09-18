#ifndef types_h
#define types_h

#include <memory>
#include <string>
#include <variant>
#include <vector>

struct ArrayBuffer {
  std::shared_ptr<uint8_t> data;
  size_t size;
};

using JSVariant = std::variant<nullptr_t, bool, int, double, long, long long,
                               std::string, ArrayBuffer>;

enum ResultType { SQLiteOk, SQLiteError };

struct BridgeResult {
  ResultType type;
  std::string message;
  int affectedRows;
  double insertId;
  std::vector<std::vector<JSVariant>> rows;
  std::vector<std::string> column_names;
};

struct BatchResult {
  ResultType type;
  std::string message;
  int affectedRows;
  int commands;
};

struct BatchArguments {
  std::string sql;
  std::shared_ptr<std::vector<JSVariant>> params;
};

#endif /* types_h */
