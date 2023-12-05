#ifndef DumbHostObject_h
#define DumbHostObject_h

#include <stdio.h>

#include "DynamicHostObject.h"
#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

class JSI_EXPORT DumbHostObject : public jsi::HostObject {
public:
  DumbHostObject(){};

  DumbHostObject(std::shared_ptr<std::vector<DynamicHostObject>> metadata);

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

  std::vector<JSVariant> values;

  std::shared_ptr<std::vector<DynamicHostObject>> metadata;
};

} // namespace opsqlite

#endif /* DumbHostObject_h */
