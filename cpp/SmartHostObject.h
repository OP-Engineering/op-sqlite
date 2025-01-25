#pragma once

#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

class JSI_EXPORT SmartHostObject : public jsi::HostObject {
  public:
    SmartHostObject() = default;

    std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt) override;

    jsi::Value get(jsi::Runtime &rt,
                   const jsi::PropNameID &propNameID) override;

    std::vector<std::pair<std::string, JSVariant>> fields;
};

} // namespace opsqlite
