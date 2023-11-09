#ifndef DynamicHostObject_h
#define DynamicHostObject_h

#include <jsi/jsi.h>
#include <any>
#include <vector>
#include "types.h"

namespace opsqlite {

    namespace jsi = facebook::jsi;

    class JSI_EXPORT DynamicHostObject: public jsi::HostObject {
    public:
        DynamicHostObject() {};

        std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

        jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

        std::vector<std::pair<std::string, JSVariant>> fields;
    };

}

#endif /* DynamicHostObject_h */
