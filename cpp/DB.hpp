#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <sqlite3.h>
#include <string>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

void invoke_generic_callback(void *cb);
jsi::Object create_db(jsi::Runtime &rt, const std::string &path);

} // namespace opsqlite
