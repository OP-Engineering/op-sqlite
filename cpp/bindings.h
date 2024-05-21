#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <jsi/jsilib.h>


namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

void install(jsi::Runtime &rt,
             std::shared_ptr<react::CallInvoker> jsCallInvoker,
             const char *docPath, const char *crsqlitePath);
void clearState();

} // namespace opsqlite
