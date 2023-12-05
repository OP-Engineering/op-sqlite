#ifndef DynamicHostObject_h
#define DynamicHostObject_h

#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

class JSI_EXPORT DynamicHostObject : public jsi::HostObject {
public:
  DynamicHostObject(){};

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

  std::vector<std::pair<std::string, JSVariant>> fields;
};

} // namespace opsqlite

#endif /* DynamicHostObject_h */
