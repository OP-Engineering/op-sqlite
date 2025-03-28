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

        // Wait until some results are returned
        if (parsedResponse.results.length === 0) {
          continue;
        }

        const allTestsPassed = parsedResponse.results.reduce((acc, r) => {
          console.log(`- ${r.description} : ${r.type}`);
          return acc && r.type !== 'incorrect';
        }, true);

        if (allTestsPassed) {
          console.log(
            `🟢🟢🟢🟢🟢 ${parsedResponse.results.length} tests passed!`
          );
          process.exit(0);
        } else {
          parsedResponse.results.forEach((r) => {
            if (r.type === 'incorrect') {
              console.log(`🟥Failed: ${JSON.stringify(r, null, 2)}`);
            }
          });
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
