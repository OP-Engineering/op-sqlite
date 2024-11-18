cd example
JAVA_OPTS=-XX:MaxHeapSize=6g yarn run:android:release --cache-dir=.turbo/android
adb forward tcp:9000 tcp:9000
echo "Polling in-app server..."
node ./scripts/poll-in-app-server.js