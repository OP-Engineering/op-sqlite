#ifndef DynamicHostObject_h
#define DynamicHostObject_h

#include <jsi/jsi.h>
#include <any>
#include <vector>
#include "types.h"

namespace osp {

    namespace jsi = facebook::jsi;

    class JSI_EXPORT DynamicHostObject: public jsi::HostObject {
    public:
        DynamicHostObject() {};

        virtual ~DynamicHostObject() {};

        std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

        jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);


        std::vector<std::pair<std::string, jsVal>> fields;
    };

}

#endif /* DynamicHostObject_h */
