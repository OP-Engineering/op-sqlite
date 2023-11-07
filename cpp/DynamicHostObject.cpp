#include "DynamicHostObject.h"
#include "utils.h"
#include <iostream>

namespace osp {

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
    
    if(fields.find(name) == fields.end()) {
        return {};
    } else {
        jsVal val = fields[name];
        return toJSI(rt, val);
    }
}

}
