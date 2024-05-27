#pragma once

#include <ReactCommon/CallInvoker.h>
#include <jsi/jsi.h>
#include <jsi/jsilib.h>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

void install(jsi::Runtime &rt, std::shared_ptr<react::CallInvoker> invoker,
             const char *base_path, const char *crsqlite_path);
void clearState();

} // namespace opsqlite
