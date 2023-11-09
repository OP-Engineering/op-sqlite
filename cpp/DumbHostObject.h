#ifndef DumbHostObject_h
#define DumbHostObject_h

#include <stdio.h>

#include <jsi/jsi.h>
#include <any>
#include <vector>
#include "types.h"
#include "DynamicHostObject.h"

namespace opsqlite {

    namespace jsi = facebook::jsi;

    class JSI_EXPORT DumbHostObject: public jsi::HostObject {
    public:
        DumbHostObject() {};
        
        DumbHostObject(std::shared_ptr<std::vector<DynamicHostObject>> metadata);

        std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

        jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

        std::vector<JSVariant> values;
        
        std::shared_ptr<std::vector<DynamicHostObject>> metadata;
    };

}

#endif /* DumbHostObject_h */
