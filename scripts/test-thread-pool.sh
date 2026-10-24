#!/bin/bash
# Builds scripts/thread_pool_check.cpp against cpp/OPThreadPool.cpp and runs it
# under ThreadSanitizer and AddressSanitizer. No React Native toolchain needed.
#
#   scripts/test-thread-pool.sh            # both sanitizers
#   scripts/test-thread-pool.sh thread     # one of them
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${TMPDIR:-/tmp}/op-sqlite-thread-pool-check"
mkdir -p "$OUT"

sanitizers=("$@")
if [ ${#sanitizers[@]} -eq 0 ]; then
  sanitizers=(thread address)
fi

for sanitizer in "${sanitizers[@]}"; do
  echo "== -fsanitize=$sanitizer"
  "${CXX:-clang++}" -std=c++17 -O1 -g -fno-omit-frame-pointer \
    -fsanitize="$sanitizer" -I "$ROOT/cpp" \
    "$ROOT/cpp/OPThreadPool.cpp" "$ROOT/scripts/thread_pool_check.cpp" \
    -o "$OUT/check-$sanitizer"
  "$OUT/check-$sanitizer"
done
