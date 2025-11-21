#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <jsi/jsilib.h>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

void install(jsi::Runtime &rt,
             const std::shared_ptr<react::CallInvoker> &invoker,
             const char *base_path, const char *crsqlite_path,
             const char *sqlite_vec_path);
void invalidate();
void expoUpdatesWorkaround(const char *base_path);

} // namespace opsqlite
