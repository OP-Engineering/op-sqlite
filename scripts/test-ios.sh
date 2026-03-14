#!/bin/bash
# set -euo pipefail

LOG_STREAM_PID=""
LOG_FILE=""

cleanup() {
  if [[ -n "${LOG_STREAM_PID}" ]]; then
    kill "${LOG_STREAM_PID}" 2>/dev/null || true
  fi
}

print_diagnostics() {
  echo "=== iOS diagnostics ==="
  local device_id
  device_id=$(xcrun simctl list devices booted | grep -m1 Booted | awk -F '[()]' '{print $2}')
  if [[ -n "${device_id}" ]]; then
    xcrun simctl spawn "${device_id}" log show --style syslog --predicate 'process == "OPSQLiteExample"' --info --debug --last 10m || true
  else
    echo "No booted simulator device found."
  fi
}

# on_error() {
#   local exit_code=$?
#   echo "❌ iOS test script failed with exit code ${exit_code}"
#   print_diagnostics
#   exit "${exit_code}"
# }

wait_for_test_result() {
  while true; do
    if grep -q "OPSQLITE_TEST_RESULT:PASS" "${LOG_FILE}"; then
      echo "🟢 iOS test suite passed (from log marker)"
      return 0
    fi

    if grep -q "OPSQLITE_TEST_RESULT:FAIL" "${LOG_FILE}"; then
      echo "🟥 iOS test suite failed (from log marker)"
      return 1
    fi

    sleep 5
  done
}

trap cleanup EXIT
# trap on_error ERR

cd example || exit

xcrun simctl boot "$(xcrun simctl list devices available | grep -m1 'Booted' || xcrun simctl list devices available | grep -m1 'Shutdown' | awk -F '[()]' '{print $2}')"

DEVICE_ID=$(xcrun simctl list devices booted | grep -m1 Booted | awk -F '[()]' '{print $2}')
if [[ -z "${DEVICE_ID}" ]]; then
  echo "No booted simulator device found after boot command"
  exit 1
fi

# Prevent the simulator from auto-locking the screen, which suspends the app
# xcrun simctl spawn "${DEVICE_ID}" defaults write com.apple.springboard idleTimerDuration -int 0 2>/dev/null || true

LOG_FILE="$(pwd)/ios-sim-log.txt"
rm -f "${LOG_FILE}"
xcrun simctl spawn "${DEVICE_ID}" log stream --style syslog --level debug --predicate 'process == "OPSQLiteExample"' | tee "${LOG_FILE}" &
LOG_STREAM_PID=$!

yarn run:ios:release

# sleep 80

wait_for_test_result


