#!/bin/bash

set -e

# Change to the iOS directory
cd "$(dirname "$0")/../ios"

echo "📦 Updating sqlitevec.xcframework binaries..."

# Device (ios-arm64)
echo "🔨 Processing device binary (ios-arm64)..."
cp vec0-ios-aarch64.dylib sqlitevec
install_name_tool -id @rpath/sqlitevec.framework/sqlitevec sqlitevec
codesign -f -s - --identifier com.op.sqlitevec sqlitevec
mv sqlitevec sqlitevec.xcframework/ios-arm64/sqlitevec.framework/

# Simulator (ios-arm64_x86_64-simulator) - create fat binary
echo "🔨 Processing simulator binaries (arm64 + x86_64)..."
lipo -create vec0-iossimulator-aarch64.dylib vec0-iossimulator-x86_64.dylib -output sqlitevec
install_name_tool -id @rpath/sqlitevec.framework/sqlitevec sqlitevec
codesign -f -s - --identifier com.op.sqlitevec sqlitevec
mv sqlitevec sqlitevec.xcframework/ios-arm64_x86_64-simulator/sqlitevec.framework/

echo "✅ sqlitevec.xcframework updated successfully!"
