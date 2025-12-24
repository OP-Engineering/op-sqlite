#!/bin/bash

cd example

xcrun simctl boot "$(xcrun simctl list devices available | grep -m1 'Booted' || xcrun simctl list devices available | grep -m1 'Shutdown' | awk -F '[()]' '{print $2}')"

yarn run:ios:release

sleep 5

cd ..

node ./scripts/poll-in-app-server.js

if [ $? -ne 0 ]; then
  echo "poll-in-app-server failed, printing device logs..."
  DEVICE_ID=$(xcrun simctl list devices booted | grep -m1 Booted | awk -F '[()]' '{print $2}')
  if [ -n "$DEVICE_ID" ]; then
    xcrun simctl spawn "$DEVICE_ID" log show --style syslog --predicate 'process == "OPSQLiteExample"' --info --debug --last 10m
  else
    echo "No booted simulator device found."
  fi
fi


