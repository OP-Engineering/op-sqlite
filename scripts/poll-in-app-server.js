const http = require('http');

async function pollInAppServer() {
  const startTime = Date.now();
  const maxDuration = 5 * 60 * 1000; // 5 minutes
  const pollInterval = 1000; // 1 second

  while (Date.now() - startTime < maxDuration) {
    try {
      const response = await makeHttpRequest('http://127.0.0.1:9000/results');

      if (response !== null) {
        let parsedResponse = JSON.parse(response);

        if (response.passed === true) {
          console.log(
            `🟢🟢🟢🟢🟢 ${parsedResponse.results.length} tests passed!`
          );
          process.exit(0);
        }
        if (response.passed === false) {
          console.log('🟥🟥🟥🟥🟥 Some tests failed');
          process.exit(1);
        }
      }
    } catch (error) {
      console.error('Error occurred during polling:', error);
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
