#ifndef jsVal_h
#define jsVal_h

#include <variant>

struct ArrayBuffer {
    std::shared_ptr<uint8_t> data;
    size_t size;
};


using jsVal = std::variant<nullptr_t, bool, int, double, long, long long, std::string, ArrayBuffer>;

#endif /* jsVal_h */
