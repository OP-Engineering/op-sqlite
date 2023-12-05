#ifndef types_h
#define types_h

#include <memory>
#include <variant>

struct ArrayBuffer {
  std::shared_ptr<uint8_t> data;
  size_t size;
};

using JSVariant = std::variant<nullptr_t, bool, int, double, long, long long,
                               std::string, ArrayBuffer>;

#endif /* types_h */
