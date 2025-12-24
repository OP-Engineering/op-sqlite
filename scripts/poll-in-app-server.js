const http = require('http');

async function pollInAppServer() {
  const startTime = Date.now();
  const maxDuration = 2 * 60 * 1000; // 3 minutes - tests can take time on CI
  const pollInterval = 5000; //

  // Do an initial ping into the server

  try {
    await makeHttpRequest('http://127.0.0.1:9000/ping')
    console.log("🟢 Ping success")
  } catch(e) {
    console.error("Ping failed!")
  }

  while (Date.now() - startTime < maxDuration) {
    try {
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

  console.error(`Polling timed out after ${Math.round(maxDuration/1000)} seconds`);
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
