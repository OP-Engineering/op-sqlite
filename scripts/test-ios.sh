#!/bin/bash

cd example || exit
yarn run run:ios:release

echo "Polling in-app server..."

node ./scripts/poll-in-app-server.js