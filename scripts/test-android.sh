#!/bin/bash
set -ex

cd example || exit

adb wait-for-device
echo "Waiting for boot to complete..."
adb shell 'while [[ -z $(getprop sys.boot_completed) ]]; do sleep 1; done'
echo "Boot completed!"
adb shell input keyevent 82
adb forward tcp:9000 tcp:9000

JAVA_OPTS=-XX:MaxHeapSize=6g yarn run:android:release

echo "Clearing old logs..."
adb logcat -c

echo "Starting app..."
JAVA_OPTS=-XX:MaxHeapSize=6g yarn run:android:release &
APP_PID=$!

echo "Waiting 15 seconds for app to initialize..."
sleep 15

echo "Checking if app is running..."
adb shell "ps | grep com.op.sqlite.example" || echo "Warning: App process not found"

node ../scripts/poll-in-app-server.js || {
  echo "❌ poll-in-app-server failed, printing device logs from app launch..."
  adb logcat -d "*:E" # Show only errors first
  echo ""
  echo "=== Full logcat from app launch ==="
  adb logcat -d | grep -E "(com.op.sqlite|ReactNative|FATAL|AndroidRuntime)" || adb logcat -d | tail -200
  kill $APP_PID 2>/dev/null || true
  exit 1
}