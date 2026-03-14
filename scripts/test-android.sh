#!/bin/bash
set -euo pipefail

APP_ID="com.op.sqlite.example"
LOGCAT_FILE=""
LOGCAT_PID=""
JS_LOG_MONITOR_PID=""

cleanup() {
  if [[ -n "${JS_LOG_MONITOR_PID}" ]]; then
    kill "${JS_LOG_MONITOR_PID}" 2>/dev/null || true
  fi

  if [[ -n "${LOGCAT_PID}" ]]; then
    kill "${LOGCAT_PID}" 2>/dev/null || true
  fi
}

start_js_log_monitor() {
  # Stream only relevant JS test lines to CI output for easier debugging.
  tail -n 0 -F "${LOGCAT_FILE}" 2>/dev/null | \
    grep --line-buffered -E "ReactNativeJS: (App has started|TESTS STARTED|TESTS FINISHED|TEST FAILED|\\[op-test\\]|OPSQLITE_TEST_RESULT)" &
  JS_LOG_MONITOR_PID=$!
}

wait_for_test_result() {
  local stall_timeout_seconds=180
  local last_progress_count=0
  local last_progress_time
  last_progress_time=$(date +%s)

  while true; do
    if grep -q "OPSQLITE_TEST_RESULT:PASS" "${LOGCAT_FILE}"; then
      echo "🟢 Test suite passed (from logcat marker)"
      return 0
    fi

    if grep -q "OPSQLITE_TEST_RESULT:FAIL" "${LOGCAT_FILE}"; then
      echo "🟥 Test suite failed (from logcat marker)"
      return 1
    fi

    local progress_count
    progress_count=$(grep -c "\\[op-test\\] Running test" "${LOGCAT_FILE}" 2>/dev/null || true)

    if [[ "${progress_count}" -gt "${last_progress_count}" ]]; then
      last_progress_count="${progress_count}"
      last_progress_time=$(date +%s)
      grep "\\[op-test\\] Running test" "${LOGCAT_FILE}" | tail -1 || true
    fi

    local now
    now=$(date +%s)

    if (( now - last_progress_time > stall_timeout_seconds )); then
      echo "⚠️ No op-test progress for ${stall_timeout_seconds}s. Capturing thread dump."
      local app_pid
      app_pid=$(adb shell pidof "${APP_ID}" 2>/dev/null | tr -d '\r' || true)

      if [[ -n "${app_pid}" ]]; then
        if ! adb shell kill -3 "${app_pid}"; then
          echo "kill -3 denied, trying debuggerd fallback"
          adb shell debuggerd -b "${app_pid}" || adb shell debuggerd "${app_pid}" || true
        fi
        sleep 3
      fi

      echo "=== Last op-test progress logs ==="
      grep "\\[op-test\\]" "${LOGCAT_FILE}" | tail -40 || true
      return 1
    fi

    sleep 10
  done

  return 0
}

print_diagnostics() {
  echo "=== Android diagnostics ==="
  adb shell pidof "${APP_ID}" || true
  adb shell dumpsys activity activities | grep -A 30 "${APP_ID}" || true
  echo ""
  echo "=== ReactNativeJS test logs ==="
  grep -E "ReactNativeJS: (App has started|TESTS STARTED|TESTS FINISHED|TEST FAILED|\\[op-test\\]|OPSQLITE_TEST_RESULT)" "${LOGCAT_FILE}" | tail -200 || true
  echo ""
  echo "=== logcat errors ==="
  adb logcat -d "*:E" || true
  if [[ -n "${LOGCAT_FILE}" && -f "${LOGCAT_FILE}" ]]; then
    echo ""
    echo "=== Tail of captured logcat (${LOGCAT_FILE}) ==="
    tail -400 "${LOGCAT_FILE}" || true
  fi
}

on_error() {
  local exit_code=$?
  echo "❌ Android test script failed with exit code ${exit_code}"
  print_diagnostics
  exit "${exit_code}"
}

trap cleanup EXIT
trap on_error ERR

cd example || exit

adb wait-for-device
echo "Waiting for boot to complete..."
adb shell 'while [[ -z $(getprop sys.boot_completed) ]]; do sleep 1; done'
echo "Boot completed!"
adb shell input keyevent 82

LOGCAT_FILE="${PWD}/android-logcat.txt"
adb logcat -c || true
adb logcat -v threadtime > "${LOGCAT_FILE}" &
LOGCAT_PID=$!
start_js_log_monitor

# JAVA_OPTS=-XX:MaxHeapSize=6g yarn run:android:release

yarn run:android:release

sleep 10

wait_for_test_result