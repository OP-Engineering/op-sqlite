#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <jsi/jsilib.h>

using namespace facebook;

namespace opsqlite {

void install(jsi::Runtime &rt,
             std::shared_ptr<react::CallInvoker> jsCallInvoker,
             const char *docPath);
void clearState();

} // namespace opsqlite
