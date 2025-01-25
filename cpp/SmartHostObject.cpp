#include "SmartHostObject.h"
#include "utils.h"

namespace opsqlite {

namespace jsi = facebook::jsi;

std::vector<jsi::PropNameID>
SmartHostObject::getPropertyNames(jsi::Runtime &rt) {
    std::vector<jsi::PropNameID> keys;

    keys.reserve(fields.size());
    for (const auto &field : fields) {
        keys.emplace_back(jsi::PropNameID::forAscii(rt, field.first));
    }

    return keys;
}

jsi::Value SmartHostObject::get(jsi::Runtime &rt,
                                const jsi::PropNameID &propNameID) {
    auto name = propNameID.utf8(rt);

    for (const auto &field : fields) {
        auto fieldName = field.first;
        if (fieldName == name) {
            return to_jsi(rt, field.second);
        }
    }

    return {};
}

} // namespace opsqlite
