const path = require('path');
const {getConfig} = require('react-native-builder-bob/babel-config');
const pkg = require('../package.json');

const root = path.resolve(__dirname, '..');

module.exports = getConfig(
  {
    presets: ['module:@react-native/babel-preset'],
    plugins: [
      'nativewind/babel',
      [
        'module-resolver',
        {
          alias: {
            stream: 'stream-browserify',
          },
        },
      ],
      'babel-plugin-transform-typescript-metadata',
      ['@babel/plugin-proposal-decorators', {legacy: true}],
    ],
  },
  {root, pkg},
);
