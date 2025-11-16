#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <sqlite3.h>
#include <string>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

class NativeDB : public jsi::NativeState {
public:
  NativeDB(std::string path, sqlite3 *db,
           std::shared_ptr<react::CallInvoker> invoker)
      : path(path), db(db), invoker(invoker) {};
  std::string path;
  sqlite3 *db;
  std::shared_ptr<react::CallInvoker> invoker;
};

jsi::Object create_db(jsi::Runtime &rt, const std::string &path);
} // namespace opsqlite
