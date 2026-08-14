#pragma once

#include <ReactCommon/CallInvoker.h>
#include <atomic>
#include <jsi/jsi.h>
#include <jsi/jsilib.h>
#include <memory>

namespace opsqlite {

namespace jsi = facebook::jsi;
namespace react = facebook::react;

// Returns this generation's liveness flag. Platform glue holds onto it
// (independent of the JS runtime) so it can be handed back to invalidate()
// when this SAME generation tears down, without needing a process-global to
// look it up -- see the comment on opsqlite::generation_alive in types.hpp.
std::shared_ptr<std::atomic<bool>>
install(jsi::Runtime &rt, const std::shared_ptr<react::CallInvoker> &invoker,
        const char *base_path, const char *sqlite_vec_path);
void invalidate(const std::shared_ptr<std::atomic<bool>> &generation_alive);
void expoUpdatesWorkaround(const char *base_path);

} // namespace opsqlite
