const http = require('http');

async function pollInAppServer() {
  const startTime = Date.now();
  const maxDuration = 5 * 60 * 1000; // 20 seconds
  const pollInterval = 1000; // 1 seconds

  while (Date.now() - startTime < maxDuration) {
    try {
      console.log('Polling in-app server for results...');
      const response = await makeHttpRequest('http://127.0.0.1:9000/results');

      if (response !== null) {
        let parsedResponse = JSON.parse(response);

        if (parsedResponse.passed === true) {
          console.log(`🟢🟢🟢🟢🟢 tests passed!`);
          process.exit(0);
        }
        if (parsedResponse.passed === false) {
          console.log('🟥🟥🟥🟥🟥 Some tests failed');
          process.exit(1);
        }
      }
    } catch (error) {
      console.error('🟥', error);
    }

    await new Promise((resolve) => setTimeout(resolve, pollInterval));
  }

  console.error('Polling failed after 5 minutes');
  process.exit(1);
}

function makeHttpRequest(url) {
  return new Promise((resolve, reject) => {
    http
      .get(url, (res) => {
        let data = '';

        res.on('data', (chunk) => {
          data += chunk;
        });

        res.on('end', () => {
          resolve(data);
        });
      })
      .on('error', (error) => {
        reject(error);
      });
  });
}

pollInAppServer();
