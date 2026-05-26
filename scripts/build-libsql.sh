#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

if [ -d "$ROOT_DIR/../libsql" ]; then
  LIBSQL_DIR="$ROOT_DIR/../libsql"
elif [ -d "$ROOT_DIR/libsql" ]; then
  LIBSQL_DIR="$ROOT_DIR/libsql"
else
  echo "[op-sqlite] libsql checkout not found. Expected at ../libsql or ./libsql"
  exit 1
fi

if [ -d "$LIBSQL_DIR/bindings/c" ]; then
  BINDINGS_DIR="$LIBSQL_DIR/bindings/c"
elif [ -d "$LIBSQL_DIR/bindings/experimental/c" ]; then
  BINDINGS_DIR="$LIBSQL_DIR/bindings/experimental/c"
else
  echo "[op-sqlite] libsql C bindings directory not found under $LIBSQL_DIR/bindings"
  exit 1
fi

GENERATED_DIR="$BINDINGS_DIR/generated"
IOS_SOURCE_DIR="$GENERATED_DIR/libsql_experimental.xcframework"
ANDROID_SOURCE_DIR="$GENERATED_DIR/jniLibs"
IOS_TARGET_DIR="$ROOT_DIR/ios/libsql_experimental.xcframework"
ANDROID_TARGET_DIR="$ROOT_DIR/android/src/main/jniLibs"
ANDROID_ABIS=(arm64-v8a armeabi-v7a x86 x86_64 include)

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[op-sqlite] Missing required command: $1"
    exit 1
  fi
}

resolve_android_ndk() {
  local candidate=""
  local sdk_root=""
  local search_roots=()
  local ndk_dirs=()

  for candidate in "${ANDROID_NDK_HOME-}" "${ANDROID_NDK_ROOT-}"; do
    if [ -n "$candidate" ] && [ -d "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  for sdk_root in "${ANDROID_SDK_ROOT-}" "${ANDROID_HOME-}" "$HOME/Library/Android/sdk"; do
    if [ -n "$sdk_root" ] && [ -d "$sdk_root" ]; then
      search_roots+=("$sdk_root")
    fi
  done

  shopt -s nullglob
  for sdk_root in "${search_roots[@]}"; do
    ndk_dirs=("$sdk_root"/ndk/*)
    if [ "${#ndk_dirs[@]}" -gt 0 ]; then
      printf '%s\n' "${ndk_dirs[@]}" | sort | tail -n 1
      shopt -u nullglob
      return 0
    fi
  done
  shopt -u nullglob

  return 1
}

echo "[op-sqlite] Using libsql checkout at: $LIBSQL_DIR"
echo "[op-sqlite] Using bindings directory at: $BINDINGS_DIR"

require_cmd make
require_cmd cargo

if ! command -v cargo-ndk >/dev/null 2>&1; then
  echo "[op-sqlite] Missing cargo-ndk. Install with: cargo install cargo-ndk"
  exit 1
fi

if ANDROID_NDK_DIR="$(resolve_android_ndk)"; then
  export ANDROID_NDK_HOME="$ANDROID_NDK_DIR"
  export ANDROID_NDK_ROOT="$ANDROID_NDK_DIR"
  echo "[op-sqlite] Using Android NDK at: $ANDROID_NDK_DIR"
else
  echo "[op-sqlite] Android NDK not found. Install one under \$ANDROID_SDK_ROOT/ndk or set ANDROID_NDK_HOME"
  exit 1
fi

echo "[op-sqlite] Building Android libsql binaries"
make -C "$BINDINGS_DIR" android

echo "[op-sqlite] Building iOS libsql binaries"
make -C "$BINDINGS_DIR" ios

if [ ! -d "$IOS_SOURCE_DIR" ]; then
  echo "[op-sqlite] Missing generated iOS artifacts at: $IOS_SOURCE_DIR"
  exit 1
fi

if [ ! -d "$ANDROID_SOURCE_DIR" ]; then
  echo "[op-sqlite] Missing generated Android artifacts at: $ANDROID_SOURCE_DIR"
  exit 1
fi

echo "[op-sqlite] Installing iOS XCFramework"
rm -rf "$IOS_TARGET_DIR"
mkdir -p "$(dirname "$IOS_TARGET_DIR")"
cp -R "$IOS_SOURCE_DIR" "$IOS_TARGET_DIR"

echo "[op-sqlite] Installing Android JNI libraries"
mkdir -p "$ANDROID_TARGET_DIR"
for abi in "${ANDROID_ABIS[@]}"; do
  if [ ! -e "$ANDROID_SOURCE_DIR/$abi" ]; then
    echo "[op-sqlite] Missing generated Android artifact: $ANDROID_SOURCE_DIR/$abi"
    exit 1
  fi

  rm -rf "$ANDROID_TARGET_DIR/$abi"
  cp -R "$ANDROID_SOURCE_DIR/$abi" "$ANDROID_TARGET_DIR/$abi"
done

echo "[op-sqlite] libsql binaries installed:"
echo "  - iOS: $IOS_TARGET_DIR"
echo "  - Android: $ANDROID_TARGET_DIR"