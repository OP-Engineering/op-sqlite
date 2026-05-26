#!/bin/bash

set -e

# Change to the iOS directory
cd "$(dirname "$0")/../ios"

echo "📦 Updating sqlitevec.xcframework binaries..."

install_name_tool -id @rpath/sqlitevec.framework/sqlitevec sqlitevec.xcframework/ios-arm64/sqlitevec.framework/sqlitevec
codesign -f -s - --identifier com.ospfranco.sqlitevec sqlitevec.xcframework/ios-arm64/sqlitevec.framework/sqlitevec

install_name_tool -id @rpath/sqlitevec.framework/sqlitevec sqlitevec.xcframework/ios-arm64_x86_64-simulator/sqlitevec.framework/sqlitevec
codesign -f -s - --identifier com.ospfranco.sqlitevec sqlitevec.xcframework/ios-arm64_x86_64-simulator/sqlitevec.framework/sqlitevec

install_name_tool -id @rpath/sqlitevec.framework/sqlitevec sqlitevec.xcframework/tvos-arm64/sqlitevec.framework/sqlitevec
codesign -f -s - --identifier com.ospfranco.sqlitevec sqlitevec.xcframework/tvos-arm64/sqlitevec.framework/sqlitevec

install_name_tool -id @rpath/sqlitevec.framework/sqlitevec sqlitevec.xcframework/tvos-arm64_x86_64-simulator/sqlitevec.framework/sqlitevec
codesign -f -s - --identifier com.ospfranco.sqlitevec sqlitevec.xcframework/tvos-arm64_x86_64-simulator/sqlitevec.framework/sqlitevec

echo "✅ sqlitevec.xcframework updated successfully!"
