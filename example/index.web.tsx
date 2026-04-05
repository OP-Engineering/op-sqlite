import { AppRegistry } from 'react-native';
import App from './src/AppWeb';
import { name as appName } from './app.json';

AppRegistry.registerComponent(appName, () => App);

const rootTag = (globalThis as any).document?.getElementById('root');
if (!rootTag) {
  throw new Error('Root element not found');
}

AppRegistry.runApplication(appName, {
  rootTag,
  initialProps: {},
});
