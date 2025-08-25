import {BridgeServer} from 'react-native-http-bridge-refurbished';

let passed: boolean | null = null;

const server = new BridgeServer('http_service', true);

server.get('/ping', async (_req, res) => {
  res.send(200, 'application/text', 'pong');
});

server.get('/results', async (_req, res) => {
  res.json({passed}, 200);
});

server.listen(9000);
console.log('Server listening on port 9000');

export function stopServer() {
  server.stop();
}

export function setServerResults(r: boolean) {
  console.log('Setting server results to', r);
  passed = r;
}
