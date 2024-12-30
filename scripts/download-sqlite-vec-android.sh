#!/bin/sh -e

SQLITEVEC_VERSION=0.1.6

function download_sqlite_vec() {
  local abi=$1
  local arch=$2
  local download_url="https://github.com/asg017/sqlite-vec/releases/download/v$SQLITEVEC_VERSION/sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz"
  if [ ! -f sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz ]; then
    wget -O sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz $download_url
    tar -xvf sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz -C ../android/src/main/jniLibs/$abi/
    mv ../android/src/main/jniLibs/$abi/vec0.so ../android/src/main/jniLibs/$abi/libsqlite_vec.so
    rm sqlite-vec-$SQLITEVEC_VERSION-loadable-android-$arch.tar.gz
  fi
}

download_sqlite_vec arm64-v8a aarch64
download_sqlite_vec armeabi-v7a armv7a
download_sqlite_vec x86 i686
download_sqlite_vec x86_64 x86_64
