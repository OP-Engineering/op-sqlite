#!/bin/bash

cd example

xcrun simctl boot "$(xcrun simctl list devices available | grep -m1 'Booted' || xcrun simctl list devices available | grep -m1 'Shutdown' | awk -F '[()]' '{print $2}')"

yarn run:ios:release

cd ..

sleep(5)

node ./scripts/poll-in-app-server.js