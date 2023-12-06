#ifndef SmartHostObject_h
#define SmartHostObject_h

#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

class JSI_EXPORT SmartHostObject : public jsi::HostObject {
public:
  SmartHostObject(){};

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

  std::vector<std::pair<std::string, JSVariant>> fields;
};

} // namespace opsqlite

#endif /* SmartHostObject_h */
