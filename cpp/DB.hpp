#include <jsi/jsi.h>
#include <string>
#include <sqlite3.h>

namespace opsqlite {

namespace jsi = facebook::jsi;
//namespace react = facebook::react;

class NativeDB: public jsi::NativeState {
public:
  NativeDB(std::string path, sqlite3 *db): path(path), db(db) {};
  sqlite3 *db;
  std::string path;
};

jsi::Object create_db(jsi::Runtime &rt, const std::string &path);
}
