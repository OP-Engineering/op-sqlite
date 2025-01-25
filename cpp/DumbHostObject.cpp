#include "DumbHostObject.h"
#include "SmartHostObject.h"
#include "utils.h"
#include <iostream>

namespace opsqlite {

namespace jsi = facebook::jsi;

DumbHostObject::DumbHostObject(
    std::shared_ptr<std::vector<SmartHostObject>> metadata) {
    this->metadata = metadata;
};

std::vector<jsi::PropNameID>
DumbHostObject::getPropertyNames(jsi::Runtime &rt) {
    std::vector<jsi::PropNameID> keys;

    for (auto field : *metadata) {
        // TODO improve this by generating the propName once on metadata
        // creation
        keys.push_back(jsi::PropNameID::forAscii(
            rt, std::get<std::string>(field.fields[0].second)));
    }

    return keys;
}

jsi::Value DumbHostObject::get(jsi::Runtime &rt,
                               const jsi::PropNameID &propNameID) {

    auto name = propNameID.utf8(rt);
    auto fields = metadata.get();
    for (int i = 0; i < fields->size(); i++) {
        auto fieldName = std::get<std::string>(fields->at(i).fields[0].second);
        if (fieldName == name) {
            return to_jsi(rt, values.at(i));
        }
    }

    for (auto pairField : ownValues) {
        if (name == pairField.first) {
            return to_jsi(rt, pairField.second);
        }
    }

    return {};
}

void DumbHostObject::set(jsi::Runtime &rt, const jsi::PropNameID &name,
                         const jsi::Value &value) {
    auto key = name.utf8(rt);
    auto fields = metadata.get();
    for (int i = 0; i < fields->size(); i++) {
        auto fieldName = std::get<std::string>(fields->at(i).fields[0].second);
        if (fieldName == key) {
            values[i] = to_variant(rt, value);
            return;
        }
    }

    for (auto pairField : ownValues) {
        if (key == pairField.first) {
            pairField.second = to_variant(rt, value);
            return;
        }
    }

    ownValues.push_back(std::make_pair(key, to_variant(rt, value)));
}

} // namespace opsqlite
