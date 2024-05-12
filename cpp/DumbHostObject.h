#ifndef DumbHostObject_h
#define DumbHostObject_h

#include <stdio.h>

#include "SmartHostObject.h"
#include "types.h"
#include <any>
#include <jsi/jsi.h>
#include <vector>

namespace opsqlite {

namespace jsi = facebook::jsi;

class JSI_EXPORT DumbHostObject : public jsi::HostObject {
public:
  DumbHostObject(){};

  DumbHostObject(std::shared_ptr<std::vector<SmartHostObject>> metadata);

  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &rt);

  jsi::Value get(jsi::Runtime &rt, const jsi::PropNameID &propNameID);

  void set(jsi::Runtime &rt, const jsi::PropNameID &name,
           const jsi::Value &value);

  std::vector<JSVariant> values;

  std::shared_ptr<std::vector<SmartHostObject>> metadata;

  std::vector<std::pair<std::string, JSVariant>> ownValues;
};

} // namespace opsqlite

#endif /* DumbHostObject_h */
