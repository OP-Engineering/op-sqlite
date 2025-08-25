#!/bin/bash

cd example || exit
yarn run run:ios:release

echo "Polling in-app server..."

cd ..

node ./scripts/poll-in-app-server.js