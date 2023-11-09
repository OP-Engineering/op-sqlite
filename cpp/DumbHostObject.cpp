#include "DumbHostObject.h"
#include "utils.h"
#include <iostream>

namespace opsqlite {

    namespace jsi = facebook::jsi;

    DumbHostObject::DumbHostObject(std::shared_ptr<std::vector<DynamicHostObject>> metadata) {
        this->metadata = metadata;
    };

    std::vector<jsi::PropNameID> DumbHostObject::getPropertyNames(jsi::Runtime &rt) {
        std::vector<jsi::PropNameID> keys;
        
        for (auto field : *metadata) {
            // TODO improve this by generating the propName once on metadata creation
            keys.push_back(jsi::PropNameID::forAscii(rt, std::get<std::string>(field.fields[0].second)));
        }
        
        return keys;
    }

    jsi::Value DumbHostObject::get(jsi::Runtime &rt, const jsi::PropNameID &propNameID) {
        auto name = propNameID.utf8(rt);
        auto fields = metadata.get();
        for(int i = 0; i < fields->size(); i++) {
            auto fieldName = std::get<std::string>(fields->at(i).fields[0].second);
            if(fieldName == name) {
                return toJSI(rt, values.at(i));
            }
        }

        return {};
    }

}
