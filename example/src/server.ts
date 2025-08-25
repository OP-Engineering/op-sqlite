import {BridgeServer} from 'react-native-http-bridge-refurbished';
let passed: boolean | null = null;

const server = new BridgeServer('http_service', true);

server.get('/ping', async (_req, _res) => {
  return {message: 'pong'};
});

server.get('/results', async (_req, _res) => {
  return {passed};
});

server.listen(9000);

export function startServer() {
  return server;
}

export function stopServer() {
  server.stop();
}

export function setServerResults(r: boolean) {
  passed = r;
}
