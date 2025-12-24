#!/bin/bash
set -ex

cd example || exit

adb wait-for-device
adb shell input keyevent 82
adb forward tcp:9000 tcp:9000

JAVA_OPTS=-XX:MaxHeapSize=6g yarn run:android:release 

node ../scripts/poll-in-app-server.js || {
  echo "❌ poll-in-app-server failed, printing device logs..."
  adb logcat -d
  exit 1
}