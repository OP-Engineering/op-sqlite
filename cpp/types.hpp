#pragma once

#include <ReactCommon/CallInvoker.h>
#include <atomic>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <variant>
#include <vector>

namespace opsqlite {

extern std::shared_ptr<facebook::react::CallInvoker> invoker;

// Liveness of the current JS runtime generation. Replaced by install() and
// cleared by invalidate(), so each generation gets its own flag rather than
// sharing one process-global bool.
//
// Whoever queues work copies the shared_ptr when the work is created, so it
// always observes ITS OWN generation's liveness. install() also returns this
// same shared_ptr so invalidate() can be handed it back directly, rather
// than reading this global -- which, during a bridgeless reload where two
// generations briefly overlap, might already have been reassigned to the
// incoming generation's flag by the time the outgoing generation's
// invalidate() runs.
extern std::shared_ptr<std::atomic<bool>> generation_alive;

struct ArrayBuffer {
  std::shared_ptr<uint8_t[]> data;
  size_t size;
};

using JSVariant = std::variant<nullptr_t, bool, int, double, long, long long,
                               std::string, ArrayBuffer>;

struct BridgeResult {
  std::string message;
  int affectedRows;
  double insertId;
  std::vector<std::vector<JSVariant>> rows;
  std::vector<std::string> column_names;
};

struct BatchResult {
  std::string message;
  int affectedRows;
  int commands;
};

struct BatchArguments {
  std::string sql;
  std::vector<JSVariant> params;
};

} // namespace opsqlite
