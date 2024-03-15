#ifndef types_h
#define types_h

#include <memory>
#include <string>
#include <variant>

enum ResultType { SQLiteOk, SQLiteError };

struct BridgeResult {
  ResultType type;
  std::string message;
  int affectedRows;
  double insertId;
};

struct BatchResult {
  ResultType type;
  std::string message;
  int affectedRows;
  int commands;
};

struct ArrayBuffer {
  std::shared_ptr<uint8_t> data;
  size_t size;
};

using JSVariant = std::variant<nullptr_t, bool, int, double, long, long long,
                               std::string, ArrayBuffer>;

#endif /* types_h */
