#include "DynamicHostObject.h"
#include "utils.h"
#include <iostream>

namespace opsqlite {

namespace jsi = facebook::jsi;

std::vector<jsi::PropNameID> DynamicHostObject::getPropertyNames(jsi::Runtime &rt) {
    std::vector<jsi::PropNameID> keys;
    
    for (auto field : fields) {
        keys.push_back(jsi::PropNameID::forAscii(rt, field.first));
    }
    
    return keys;
}

jsi::Value DynamicHostObject::get(jsi::Runtime &rt, const jsi::PropNameID &propNameID) {
    auto name = propNameID.utf8(rt);
    
    for (auto field: fields) {
        auto fieldName = field.first;
        if(fieldName == name) {
            return toJSI(rt, field.second);
        }
    }
    
    return {};
}

}
