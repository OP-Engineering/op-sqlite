#pragma once

#include <cstdint>
#include <string>

namespace opsqlite {

struct RBUResult {
  std::string status;
  std::uint64_t steps;
  double progress;
  std::string state;
};

bool is_rbu_enabled();

RBUResult apply_rbu(const std::string &target_path,
                    const std::string &update_path,
                    const std::string &state_path, std::uint64_t max_steps);

} // namespace opsqlite
