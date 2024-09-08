JAVA_OPTS=-XX:MaxHeapSize=6g yarn turbo run android --cache-dir=.turbo/android
node ./scripts/poll-in-app-server.js