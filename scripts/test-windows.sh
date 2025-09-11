#!/bin/bash
set -e

echo "Building op-sqlite for Windows..."

# Navigate to windows directory
cd windows

# Clean previous builds
echo "Cleaning previous builds..."
if command -v msbuild &> /dev/null; then
    msbuild /t:Clean /p:Configuration=Debug /p:Platform=x64 OPSQLite.sln
    msbuild /t:Clean /p:Configuration=Release /p:Platform=x64 OPSQLite.sln
else
    echo "MSBuild not found. Please ensure Visual Studio is installed and MSBuild is in PATH"
    exit 1
fi

# Build Debug configuration
echo "Building Debug configuration..."
msbuild /p:Configuration=Debug /p:Platform=x64 OPSQLite.sln

# Build Release configuration
echo "Building Release configuration..."
msbuild /p:Configuration=Release /p:Platform=x64 OPSQLite.sln

echo "Windows build completed successfully!"

# Return to root directory
cd ..

# Run the example app if requested
if [ "$1" == "--run-example" ]; then
    echo "Running example app on Windows..."
    cd example
    npx react-native run-windows
fi
