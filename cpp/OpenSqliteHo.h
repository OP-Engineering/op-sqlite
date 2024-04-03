//
// Created by jplc on 4/2/24.
//

#ifndef OPSQLITEEXAMPLE_OPENSQLITEHO_H
#define OPSQLITEEXAMPLE_OPENSQLITEHO_H

#include <jsi/jsi.h>
#include <string>

namespace opsqlite {

class JSI_EXPORT OpenSqliteHo : public facebook::jsi::HostObject {
public:
  static facebook::jsi::Function open(facebook::jsi::Runtime &rt,
                                      const std::string &basePath);

private:
  static bool isParametersNumberNotOk(std::string &msg, int count);
  static std::string getPath(facebook::jsi::Runtime &rt,
                             const facebook::jsi::Object &options,
                             const std::string &basePath);
  static std::string getLocation(facebook::jsi::Runtime &rt,
                                 const facebook::jsi::Object &options);
  static std::string getEncryptionKey(facebook::jsi::Runtime &rt,
                                      const facebook::jsi::Object &options);
};

} // namespace opsqlite

#endif // OPSQLITEEXAMPLE_OPENSQLITEHO_H
