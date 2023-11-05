import {Buffer} from '@craftzdog/react-native-buffer';
import {AppRegistry} from 'react-native';
import App from './src/App';
import {name as appName} from './app.json';

global.Buffer = Buffer;
global.process.cwd = () => 'sxsx';
global.process.env = {NODE_ENV: 'production'};
global.location = {};

AppRegistry.registerComponent(appName, () => App);
