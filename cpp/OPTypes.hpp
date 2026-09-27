#pragma once

#include <ReactCommon/CallInvoker.h>
#include <atomic>
#include <memory>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <utility>
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

/// A failure reported by SQLite itself, carrying the result codes of the call
/// that failed.
///
/// Message text cannot be used to tell failures apart: extensions substitute
/// their own strings (FTS5 reports corruption as "fts5: corruption found
/// reading blob ..."), and a primary code alone cannot distinguish
/// SQLITE_IOERR_FSYNC from SQLITE_IOERR_READ or SQLITE_CORRUPT from
/// SQLITE_CORRUPT_VTAB. The codes therefore travel with the exception, and the
/// JSI layer puts them on the JS Error as `code` / `extendedCode`.
class SQLiteError : public std::runtime_error {
public:
  SQLiteError(std::string message, int code, int extended_code)
      : std::runtime_error(std::move(message)), code(code),
        extended_code(extended_code) {}

  /// Primary result code, e.g. SQLITE_CORRUPT (11).
  int code;
  /// Extended result code, e.g. SQLITE_CORRUPT_VTAB (267). Equal to `code`
  /// when SQLite has no more specific code for the failure.
  int extended_code;
};

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
