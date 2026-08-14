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
extern bool invalidated;

// Liveness of the current JS runtime generation. Replaced by install() and
// cleared by invalidate(), so each generation gets its own flag rather than
// sharing the process-global `invalidated` bool.
//
// Whoever queues work copies the shared_ptr when the work is created, so it
// always observes ITS OWN generation's liveness. Checking a process-global
// instead is wrong in both directions during a bridgeless reload, where two
// generations overlap: an outgoing generation clearing it would suppress the
// incoming generation's callbacks, and an incoming generation setting it would
// re-enable the outgoing generation's.
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
