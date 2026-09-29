#include "OPRBU.hpp"

#include "OPTypes.hpp"
#include <sqlite3.h>
#include <sys/stat.h>

#if defined(SQLITE_ENABLE_RBU) && !defined(OP_SQLITE_USE_SQLCIPHER) &&         \
    !defined(OP_SQLITE_USE_LIBSQL) && !defined(OP_SQLITE_USE_TURSO) &&         \
    !defined(OP_SQLITE_USE_PHONE_VERSION)
#define OP_SQLITE_RBU_AVAILABLE 1

extern "C" {
typedef struct sqlite3rbu sqlite3rbu;

sqlite3rbu *sqlite3rbu_open(const char *target_path, const char *update_path,
                            const char *state_path);
int sqlite3rbu_step(sqlite3rbu *rbu);
int sqlite3rbu_close(sqlite3rbu *rbu, char **error_message);
sqlite3_int64 sqlite3rbu_progress(sqlite3rbu *rbu);
int sqlite3rbu_state(sqlite3rbu *rbu);
}
#else
#define OP_SQLITE_RBU_AVAILABLE 0
#endif

namespace opsqlite {

namespace {

[[noreturn]] void throw_rbu_error(int code, const std::string &message) {
  throw OPSQLiteError(code, "[op-sqlite][RBU] " + message);
}

#if OP_SQLITE_RBU_AVAILABLE

constexpr int RBU_STATE_OAL = 1;
constexpr int RBU_STATE_MOVE = 2;
constexpr int RBU_STATE_CHECKPOINT = 3;
constexpr int RBU_STATE_DONE = 4;
constexpr int RBU_STATE_ERROR = 5;

bool is_regular_file(const std::string &path) {
  struct stat info {};
  return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool directory_exists(const std::string &path) {
  struct stat info {};
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

std::string parent_directory(const std::string &path) {
  const auto separator = path.find_last_of('/');
  if (separator == std::string::npos) {
    return {};
  }
  if (separator == 0) {
    return "/";
  }
  return path.substr(0, separator);
}

std::string state_name(int state) {
  switch (state) {
  case RBU_STATE_OAL:
    return "oal";
  case RBU_STATE_MOVE:
    return "move";
  case RBU_STATE_CHECKPOINT:
    return "checkpoint";
  case RBU_STATE_DONE:
    return "done";
  case RBU_STATE_ERROR:
    return "error";
  default:
    return "unknown";
  }
}

void validate_paths(const std::string &target_path,
                    const std::string &update_path,
                    const std::string &state_path) {
  if (target_path.empty() || target_path.front() != '/') {
    throw_rbu_error(SQLITE_MISUSE, "targetPath must be an absolute path");
  }
  if (update_path.empty() || update_path.front() != '/') {
    throw_rbu_error(SQLITE_MISUSE, "updatePath must be an absolute path");
  }
  if (!is_regular_file(target_path)) {
    throw_rbu_error(SQLITE_CANTOPEN,
                    "target database does not exist or is not a file: " +
                        target_path);
  }
  if (!is_regular_file(update_path)) {
    throw_rbu_error(SQLITE_CANTOPEN,
                    "RBU update database does not exist or is not a file: " +
                        update_path);
  }
  if (target_path == update_path ||
      (!state_path.empty() &&
       (state_path == target_path || state_path == update_path))) {
    throw_rbu_error(SQLITE_MISUSE,
                    "targetPath, updatePath, and statePath must be distinct");
  }
  if (!state_path.empty()) {
    if (state_path.front() != '/') {
      throw_rbu_error(SQLITE_MISUSE, "statePath must be an absolute path");
    }
    const auto parent = parent_directory(state_path);
    if (!parent.empty() && !directory_exists(parent)) {
      throw_rbu_error(SQLITE_CANTOPEN,
                      "statePath parent directory does not exist: " + parent);
    }
  }
}

#endif

} // namespace

bool is_rbu_enabled() { return OP_SQLITE_RBU_AVAILABLE == 1; }

RBUResult apply_rbu(const std::string &target_path,
                    const std::string &update_path,
                    const std::string &state_path, std::uint64_t max_steps) {
#if OP_SQLITE_RBU_AVAILABLE
  validate_paths(target_path, update_path, state_path);

  sqlite3rbu *rbu =
      sqlite3rbu_open(target_path.c_str(), update_path.c_str(),
                      state_path.empty() ? nullptr : state_path.c_str());
  if (rbu == nullptr) {
    throw_rbu_error(SQLITE_NOMEM, "could not allocate an RBU handle");
  }

  int step_code = SQLITE_OK;
  std::uint64_t steps = 0;
  while (step_code == SQLITE_OK && steps < max_steps) {
    step_code = sqlite3rbu_step(rbu);
    ++steps;
  }

  const auto progress = static_cast<double>(sqlite3rbu_progress(rbu));
  const auto state = state_name(sqlite3rbu_state(rbu));
  char *error_message = nullptr;
  const int close_code = sqlite3rbu_close(rbu, &error_message);

  if (close_code != SQLITE_OK && close_code != SQLITE_DONE) {
    const std::string message =
        error_message != nullptr ? error_message : sqlite3_errstr(close_code);
    sqlite3_free(error_message);
    throw_rbu_error(close_code, message);
  }

  sqlite3_free(error_message);
  return {close_code == SQLITE_DONE ? "complete" : "paused", steps, progress,
          state};
#else
  (void)target_path;
  (void)update_path;
  (void)state_path;
  (void)max_steps;
  throw_rbu_error(
      SQLITE_MISUSE,
      "RBU is unavailable. Enable the bundled SQLite backend with "
      "\"op-sqlite\": { \"rbu\": true } and rebuild the native app");
#endif
}

} // namespace opsqlite
