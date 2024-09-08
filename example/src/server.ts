import {BridgeServer} from 'react-native-http-bridge-refurbished';
let results: any[] = [];

const server = new BridgeServer('http_service', true);

server.get('/ping', async (_req, _res) => {
  // console.log("🟦 🟦🟦🟦🟦🟦🟦 🟦 Received request for '/ping'");
  return {message: 'pong'};
});

server.get('/results', async (req, _res) => {
  // console.log("🟦 🟦🟦🟦🟦🟦🟦 🟦 Received request for '/test_results'");
  // console.log(req.type);
  // console.log(req.data);
  // console.log(req.url);
  // if (results.length > 0) {
  //   return {results};
  // } else {
  //   return {};
  // }
  return {results};
});

server.listen(9000);

export function startServer() {
  return server;
}

export function stopServer() {
  server.stop();
}

export function setServerResults(r: any[]) {
  results = r;
}
