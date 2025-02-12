#!/bin/sh -e

SQLITEVEC_VERSION=$1

echo "SQLITEVEC_VERSION: $SQLITEVEC_VERSION"

if [ ! -d sqlite-vec-$SQLITEVEC_VERSION ]; then
  wget -O sqlite-vec.tar.gz "https://github.com/asg017/sqlite-vec/archive/refs/tags/v$SQLITEVEC_VERSION.tar.gz"
  tar -xvf sqlite-vec.tar.gz
  rm sqlite-vec.tar.gz

  cd sqlite-vec-$SQLITEVEC_VERSION
  ./scripts/vendor.sh
  make sqlite-vec.h
  cd ..
fi

MIN_IOS_VERSION=13.0

function verify_framework() {
    local framework_path=$1
    echo "Verifying $framework_path"

    # Check minimum OS version
    MIN_OS_VERSION=$(otool -l "$framework_path" | grep "minos")
    CHECK_VERSION=$MIN_IOS_VERSION
    # NOTE: arm64 simulator minos is 14.0
    if [[ "$framework_path" == *"arm64_simulator"* ]]; then
        CHECK_VERSION=14.0
    fi
    if [[ "$MIN_OS_VERSION" != *"$CHECK_VERSION"* ]]; then
        echo "Error: $platform framework is not $CHECK_VERSION ($framework_path)"
        exit 1
    fi
}

IOS_SDK_PATH=$(xcrun --sdk iphoneos --show-sdk-path)
IOS_SIMULATOR_SDK_PATH=$(xcrun --sdk iphonesimulator --show-sdk-path)

CC_ios_arm64=$(xcrun --sdk iphoneos --find clang)
CC_ios_x86_64=$(xcrun --sdk iphonesimulator --find clang)

IOS_CFLAGS="-Ivendor/ -I./ -O3 -fembed-bitcode -fPIC"
IOS_LDFLAGS="-Wl,-ios_version_min,$MIN_IOS_VERSION"
IOS_ARM64_FLAGS="-target arm64-apple-ios$MIN_IOS_VERSION -miphoneos-version-min=$MIN_IOS_VERSION"
IOS_ARM64_SIM_FLAGS="-target arm64-apple-ios-simulator$MIN_IOS_VERSION -mios-simulator-version-min=$MIN_IOS_VERSION"
IOS_X86_64_FLAGS="-target x86_64-apple-ios-simulator$MIN_IOS_VERSION -mios-simulator-version-min=$MIN_IOS_VERSION"

cd sqlite-vec-$SQLITEVEC_VERSION

OUT_DIR_ios_arm64=ios/arm64
OUT_DIR_ios_x86_64=ios/x86_64
OUT_DIR_ios_arm64_simulator=ios/arm64_simulator

mkdir -p $OUT_DIR_ios_arm64
mkdir -p $OUT_DIR_ios_x86_64
mkdir -p $OUT_DIR_ios_arm64_simulator

function build_ios_arm64() {
  $CC_ios_arm64 $CFLAGS $IOS_CFLAGS $IOS_ARM64_FLAGS -isysroot $IOS_SDK_PATH -c sqlite-vec.c -o $OUT_DIR_ios_arm64/sqlite-vec.o
  $CC_ios_arm64 -dynamiclib -o $OUT_DIR_ios_arm64/sqlitevec $OUT_DIR_ios_arm64/sqlite-vec.o -isysroot $IOS_SDK_PATH $IOS_LDFLAGS
}

function build_ios_x86_64() {
  $CC_ios_x86_64 $CFLAGS $IOS_CFLAGS $IOS_X86_64_FLAGS -isysroot $IOS_SIMULATOR_SDK_PATH -c sqlite-vec.c -o $OUT_DIR_ios_x86_64/sqlite-vec.o
  $CC_ios_x86_64 $IOS_X86_64_FLAGS -dynamiclib -o $OUT_DIR_ios_x86_64/sqlitevec $OUT_DIR_ios_x86_64/sqlite-vec.o -isysroot $IOS_SIMULATOR_SDK_PATH
}

function build_ios_arm64_simulator() {
  $CC_ios_arm64 $CFLAGS $IOS_CFLAGS $IOS_ARM64_SIM_FLAGS -isysroot $IOS_SIMULATOR_SDK_PATH -c sqlite-vec.c -o $OUT_DIR_ios_arm64_simulator/sqlite-vec.o
  $CC_ios_arm64 $IOS_ARM64_SIM_FLAGS -dynamiclib -o $OUT_DIR_ios_arm64_simulator/sqlitevec $OUT_DIR_ios_arm64_simulator/sqlite-vec.o -isysroot $IOS_SIMULATOR_SDK_PATH
}

build_ios_arm64
build_ios_x86_64
build_ios_arm64_simulator

mkdir -p ios/sim_fat/
lipo -create ./ios/x86_64/sqlitevec ./ios/arm64_simulator/sqlitevec -output ios/sim_fat/sqlitevec

cp ./ios/arm64/sqlitevec ../../ios/sqlitevec.xcframework/ios-arm64/sqlitevec.framework/
install_name_tool -id @rpath/sqlitevec.framework/sqlitevec ../../ios/sqlitevec.xcframework/ios-arm64/sqlitevec.framework/sqlitevec

cp ./ios/sim_fat/sqlitevec ../../ios/sqlitevec.xcframework/ios-arm64_x86_64-simulator/sqlitevec.framework/
install_name_tool -id @rpath/sqlitevec.framework/sqlitevec ../../ios/sqlitevec.xcframework/ios-arm64_x86_64-simulator/sqlitevec.framework/sqlitevec

verify_framework ./ios/arm64/sqlitevec
verify_framework ./ios/x86_64/sqlitevec
verify_framework ./ios/arm64_simulator/sqlitevec

TVOS_SDK_PATH=$(xcrun --sdk appletvos --show-sdk-path)
TVOS_SIMULATOR_SDK_PATH=$(xcrun --sdk appletvsimulator --show-sdk-path)

CC_tvos_arm64=$(xcrun --sdk appletvos --find clang)
CC_tvos_x86_64=$(xcrun --sdk appletvsimulator --find clang)

TVOS_CFLAGS="-Ivendor/ -I./ -O3 -fembed-bitcode -fPIC"
TVOS_ARM64_FLAGS="-target arm64-apple-tvos$MIN_IOS_VERSION -mappletvos-version-min=$MIN_IOS_VERSION"
TVOS_ARM64_SIM_FLAGS="-target arm64-apple-tvos-simulator$MIN_IOS_VERSION -mappletvsimulator-version-min=$MIN_IOS_VERSION"
TVOS_X86_64_FLAGS="-target x86_64-apple-tvos-simulator$MIN_IOS_VERSION -mappletvsimulator-version-min=$MIN_IOS_VERSION"

OUT_DIR_tvos_arm64=tvos/arm64
OUT_DIR_tvos_x86_64=tvos/x86_64
OUT_DIR_tvos_arm64_simulator=tvos/arm64_simulator

mkdir -p $OUT_DIR_tvos_arm64
mkdir -p $OUT_DIR_tvos_x86_64
mkdir -p $OUT_DIR_tvos_arm64_simulator

function build_tvos_arm64() {
  $CC_tvos_arm64 $CFLAGS $TVOS_CFLAGS $TVOS_ARM64_FLAGS -isysroot $TVOS_SDK_PATH -c sqlite-vec.c -o $OUT_DIR_tvos_arm64/sqlite-vec.o
  $CC_tvos_arm64 $TVOS_ARM64_FLAGS -dynamiclib -o $OUT_DIR_tvos_arm64/sqlitevec $OUT_DIR_tvos_arm64/sqlite-vec.o -isysroot $TVOS_SDK_PATH
}

function build_tvos_x86_64() {
  $CC_tvos_x86_64 $CFLAGS $TVOS_CFLAGS $TVOS_X86_64_FLAGS -isysroot $TVOS_SIMULATOR_SDK_PATH -c sqlite-vec.c -o $OUT_DIR_tvos_x86_64/sqlite-vec.o
  $CC_tvos_x86_64 $TVOS_X86_64_FLAGS -dynamiclib -o $OUT_DIR_tvos_x86_64/sqlitevec $OUT_DIR_tvos_x86_64/sqlite-vec.o -isysroot $TVOS_SIMULATOR_SDK_PATH
}

function build_tvos_arm64_simulator() {
  $CC_tvos_arm64 $CFLAGS $TVOS_CFLAGS $TVOS_ARM64_SIM_FLAGS -isysroot $TVOS_SIMULATOR_SDK_PATH -c sqlite-vec.c -o $OUT_DIR_tvos_arm64_simulator/sqlite-vec.o
  $CC_tvos_arm64 $TVOS_ARM64_SIM_FLAGS -dynamiclib -o $OUT_DIR_tvos_arm64_simulator/sqlitevec $OUT_DIR_tvos_arm64_simulator/sqlite-vec.o -isysroot $TVOS_SIMULATOR_SDK_PATH
}

build_tvos_arm64
build_tvos_x86_64
build_tvos_arm64_simulator

mkdir -p tvos/sim_fat/
lipo -create ./tvos/x86_64/sqlitevec ./tvos/arm64_simulator/sqlitevec -output tvos/sim_fat/sqlitevec

cp ./tvos/arm64/sqlitevec ../../ios/sqlitevec.xcframework/tvos-arm64/sqlitevec.framework/
install_name_tool -id @rpath/sqlitevec.framework/sqlitevec ../../ios/sqlitevec.xcframework/tvos-arm64/sqlitevec.framework/sqlitevec

cp ./tvos/sim_fat/sqlitevec ../../ios/sqlitevec.xcframework/tvos-arm64_x86_64-simulator/sqlitevec.framework/
install_name_tool -id @rpath/sqlitevec.framework/sqlitevec ../../ios/sqlitevec.xcframework/tvos-arm64_x86_64-simulator/sqlitevec.framework/sqlitevec

verify_framework ./tvos/arm64/sqlitevec
verify_framework ./tvos/x86_64/sqlitevec
verify_framework ./tvos/arm64_simulator/sqlitevec

cd ..
function download_sqlite_vec_android() {
  local abi=$1
  local arch=$2
  local download_url="https://github.com/asg017/sqlite-vec/releases/download/v$SQLITEVEC_VERSION/sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz"
  if [ ! -f sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz ]; then
    wget -O sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz $download_url
    tar -xvf sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz
    mv vec0.so ../android/src/main/jniLibs/$abi/libsqlite_vec.so
    rm sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz
  fi
}

download_sqlite_vec_android arm64-v8a aarch64
download_sqlite_vec_android armeabi-v7a armv7a
download_sqlite_vec_android x86 i686
download_sqlite_vec_android x86_64 x86_64
