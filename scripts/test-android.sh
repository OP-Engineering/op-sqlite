JAVA_OPTS=-XX:MaxHeapSize=6g yarn turbo run run:android:release --cache-dir=.turbo/android
adb forward tcp:9000 tcp:9000
# echo "🟦 Android device address:"
adb shell ip addr show
# echo "📱 Android Emulator IP Address:"
# adb shell ifconfig | grep "inet addr" | awk '{print $2}' | awk -F: '{print $2}'
echo "Polling in-app server..."
node ./scripts/poll-in-app-server.js