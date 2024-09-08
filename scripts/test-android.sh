JAVA_OPTS=-XX:MaxHeapSize=6g yarn turbo run run:android:release --cache-dir=.turbo/android
adb reverse tcp:10424 tcp:10424
node ./scripts/poll-in-app-server.js