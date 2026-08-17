const path = require('path');
const pkg = require('../package.json');

module.exports = {
  assets: ['./assets/'],
  project: {
    ios: {
      // Auto pod-install conflicts with the SwiftPM setup (`npx react-native
      // spm`) — it silently re-integrates CocoaPods into the .xcodeproj on
      // every `npm run ios`, undoing `spm add --deintegrate`. Re-enable this
      // (and run `pod install`) if you deliberately revert to CocoaPods via
      // `npx react-native spm deinit`.
      automaticPodsInstallation: false,
    },
  },
  dependencies: {
    [pkg.name]: {
      root: path.join(__dirname, '..'),
      platforms: {
        // Codegen script incorrectly fails without this
        // So we explicitly specify the platforms with empty object
        ios: {},
        android: {},
      },
    },
  },
};
