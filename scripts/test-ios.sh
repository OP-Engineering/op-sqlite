#!/bin/bash

yarn run run:ios
echo "Polling in-app server..."
node ./scripts/poll-in-app-server.js