#pragma once

#include "SmartHostObject.h"
#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <stdio.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

class JSI_EXPORT DumbHostObject : public jsi::HostObject {
  public:
    DumbHostObject() = default;

    explicit DumbHostObject(
        std::shared_ptr<std::vector<SmartHostObject>> metadata);

    std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt) override;

    jsi::Value get(jsi::Runtime &rt,
                   const jsi::PropNameID &propNameID) override;

    void set(jsi::Runtime &rt, const jsi::PropNameID &name,
             const jsi::Value &value) override;

    std::vector<JSVariant> values;

    std::shared_ptr<std::vector<SmartHostObject>> metadata;

    std::vector<std::pair<std::string, JSVariant>> ownValues;
};

} // namespace opsqlite
