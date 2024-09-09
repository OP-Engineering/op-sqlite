# xcrun simctl boot "iPhone 15 Pro"
yarn turbo run run:ios --cache-dir=.turbo/ios
echo "Polling in-app server..."
node ./scripts/poll-in-app-server.js