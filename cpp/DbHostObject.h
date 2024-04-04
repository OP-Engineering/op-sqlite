//
// Created by jplc on 4/2/24.
//

#ifndef OPSQLITEEXAMPLE_DBHOSTOBJECT_H
#define OPSQLITEEXAMPLE_DBHOSTOBJECT_H

#include <jsi/jsi.h>
#include <string>

namespace jsi = facebook::jsi;

namespace opsqlite {

class JSI_EXPORT DbHostObject : public jsi::HostObject {
public:
  jsi::Value get(jsi::Runtime &runtime,
                 const jsi::PropNameID &propNameId) override;
  void set(jsi::Runtime &runtime, const jsi::PropNameID &propNameId,
           const jsi::Value &value) override;
  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime &runtime) override;

  static jsi::Function open(jsi::Runtime &rt, const std::string &basePath);
  static jsi::Function attach(jsi::Runtime &rt, const std::string &basePath);

private:
  static const std::string F_OPEN; // open
  static const int F_OPEN_ARGS_COUNT;
  static const std::string F_ATTACH; // open
  static const int F_ATTACH_ARGS_COUNT;
};

} // namespace opsqlite

#endif // OPSQLITEEXAMPLE_DBHOSTOBJECT_H
