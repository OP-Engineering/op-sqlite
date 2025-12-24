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

echo "Waiting 10 seconds for app to start..."
sleep 10 

node ../scripts/poll-in-app-server.js || {
  echo "❌ poll-in-app-server failed, printing device logs..."
  adb logcat -d
  exit 1
}