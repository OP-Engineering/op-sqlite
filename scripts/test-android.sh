JAVA_OPTS=-XX:MaxHeapSize=6g yarn turbo run android --cache-dir="${{ env.TURBO_CACHE_DIR }}"
node ./scripts/poll-in-app-server.js