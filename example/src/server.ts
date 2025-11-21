import {HttpServer} from '@op-engineering/op-server';

let passed: boolean | null = null;

const server = new HttpServer();

server.get('/ping', async (_req, res) => {
  res.statusCode = 200;
  res.contentType = 'application/text';
  res.content = 'pong';
  // res.send(200, 'application/text', 'pong');
});

server.get('/results', async (_req, res) => {
  res.statusCode = 200;
  res.content = JSON.stringify({passed});
  // res.json({passed}, 200);
});

server.listen(9000);
// console.log('Server listening on port 9000');

export function stopServer() {
  server.stop();
}

export function setServerResults(r: boolean) {
  // console.log('Setting server results to', r);
  passed = r;
}
