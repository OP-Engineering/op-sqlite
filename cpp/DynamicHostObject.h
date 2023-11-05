#ifndef DynamicHostObject_h
#define DynamicHostObject_h

#include <stdio.h>
#include <jsi/jsi.h>
#include <any>
#include <vector>

namespace osp {

namespace jsi = facebook::jsi;

class JSI_EXPORT DynamicHostObject: public jsi::HostObject {
public:
    DynamicHostObject() {};
    
    std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);
    
    jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);
    
    std::unordered_map<std::string, std::any> fields;
};

}

#endif /* DynamicHostObject_h */
