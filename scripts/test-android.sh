JAVA_OPTS=-XX:MaxHeapSize=6g yarn turbo run run:android:release --cache-dir=.turbo/android
node ./scripts/poll-in-app-server.js