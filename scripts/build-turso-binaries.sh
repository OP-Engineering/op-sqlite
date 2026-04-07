#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"

# Prefer sibling clone, fall back to in-repo checkout.
if [ -d "$ROOT_DIR/../turso" ]; then
  TURSO_DIR="$ROOT_DIR/../turso"
elif [ -d "$ROOT_DIR/turso" ]; then
  TURSO_DIR="$ROOT_DIR/turso"
else
  echo "[op-sqlite] Turso checkout not found. Expected at ../turso or ./turso"
  exit 1
fi

IOS_TARGET_DIR="$ROOT_DIR/ios/turso_sdk_kit.xcframework"
ANDROID_TARGET_DIR="$ROOT_DIR/android/src/main/tursoLibs"
RUST_PACKAGE="turso_sync_sdk_kit"
RUST_LIB_BASENAME="turso_sync_sdk_kit"
ANDROID_INCLUDE_DIR="$ANDROID_TARGET_DIR/include"

IOS_DEVICE_TRIPLE="aarch64-apple-ios"
IOS_SIM_ARM64_TRIPLE="aarch64-apple-ios-sim"
IOS_SIM_X64_TRIPLE="x86_64-apple-ios"

ANDROID_TRIPLES=(
  "aarch64-linux-android"
  "armv7-linux-androideabi"
  "i686-linux-android"
  "x86_64-linux-android"
)

android_abi_from_triple() {
  case "$1" in
    aarch64-linux-android) echo "arm64-v8a" ;;
    armv7-linux-androideabi) echo "armeabi-v7a" ;;
    i686-linux-android) echo "x86" ;;
    x86_64-linux-android) echo "x86_64" ;;
    *)
      echo "Unknown Android triple: $1" >&2
      exit 1
      ;;
  esac
}

require_cmd() {
  if ! command -v "$1" >/dev/null 2>&1; then
    echo "[op-sqlite] Missing required command: $1"
    exit 1
  fi
}

echo "[op-sqlite] Using Turso checkout at: $TURSO_DIR"

require_cmd rustup
require_cmd cargo
require_cmd xcodebuild
require_cmd lipo

if ! command -v cargo-ndk >/dev/null 2>&1; then
  echo "[op-sqlite] Missing cargo-ndk. Install with: cargo install cargo-ndk"
  exit 1
fi

echo "[op-sqlite] Adding Rust targets"
rustup target add "$IOS_DEVICE_TRIPLE" "$IOS_SIM_ARM64_TRIPLE" "$IOS_SIM_X64_TRIPLE"
for triple in "${ANDROID_TRIPLES[@]}"; do
  rustup target add "$triple"
done

pushd "$TURSO_DIR" >/dev/null

echo "[op-sqlite] Building iOS Turso SDK kit"
cargo build --release --package "$RUST_PACKAGE" --target "$IOS_DEVICE_TRIPLE"
cargo build --release --package "$RUST_PACKAGE" --target "$IOS_SIM_ARM64_TRIPLE"

SIM_X64_BUILT=0
if cargo build --release --package "$RUST_PACKAGE" --target "$IOS_SIM_X64_TRIPLE"; then
  SIM_X64_BUILT=1
else
  echo "[op-sqlite] x86_64 iOS simulator build failed, continuing with arm64 simulator only"
fi

echo "[op-sqlite] Building Android Turso SDK kit"
for triple in "${ANDROID_TRIPLES[@]}"; do
  cargo ndk --target "$triple" --platform 31 build --release --package "$RUST_PACKAGE"
done

popd >/dev/null

echo "[op-sqlite] Packaging iOS XCFramework"
TMP_DIR="$ROOT_DIR/.tmp/turso-build"
rm -rf "$TMP_DIR" "$IOS_TARGET_DIR"
mkdir -p "$TMP_DIR" "$ANDROID_INCLUDE_DIR"

FRAMEWORK_NAME="turso_sdk_kit"
BUNDLE_ID="com.turso.turso-sdk-kit"
HEADER_SRC="$TURSO_DIR/sdk-kit/turso.h"
SYNC_HEADER_SRC="$TURSO_DIR/sync/sdk-kit/turso_sync.h"

make_framework() {
  local slice_dir="$1"   # destination directory for the .framework
  local dylib_src="$2"   # path to the (possibly fat) compiled dylib

  local fw_dir="$slice_dir/${FRAMEWORK_NAME}.framework"
  mkdir -p "$fw_dir/Headers"

  # Dylib goes in the bundle named without extension
  cp "$dylib_src" "$fw_dir/$FRAMEWORK_NAME"
  install_name_tool -id "@rpath/${FRAMEWORK_NAME}.framework/${FRAMEWORK_NAME}" "$fw_dir/$FRAMEWORK_NAME"
  codesign -f -s - --identifier "$BUNDLE_ID" "$fw_dir/$FRAMEWORK_NAME"

  cp "$HEADER_SRC" "$fw_dir/Headers/"
  if [ -f "$SYNC_HEADER_SRC" ]; then
    cp "$SYNC_HEADER_SRC" "$fw_dir/Headers/"
  fi

  cat > "$fw_dir/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>en</string>
  <key>CFBundleExecutable</key>
  <string>${FRAMEWORK_NAME}</string>
  <key>CFBundleIdentifier</key>
  <string>${BUNDLE_ID}</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundlePackageType</key>
  <string>FMWK</string>
  <key>CFBundleSignature</key>
  <string>????</string>
  <key>CFBundleVersion</key>
  <string>1.0.0</string>
  <key>CFBundleShortVersionString</key>
  <string>1.0.0</string>
  <key>MinimumOSVersion</key>
  <string>13.0</string>
</dict>
</plist>
PLIST
}

# Device slice
DEVICE_SLICE_DIR="$TMP_DIR/ios-arm64"
IOS_DEVICE_LIB="$TURSO_DIR/target/$IOS_DEVICE_TRIPLE/release/lib${RUST_LIB_BASENAME}.dylib"
make_framework "$DEVICE_SLICE_DIR" "$IOS_DEVICE_LIB"

# Simulator slice (fat binary)
SIM_SLICE_DIR="$TMP_DIR/ios-arm64_x86_64-simulator"
IOS_SIM_ARM64_LIB="$TURSO_DIR/target/$IOS_SIM_ARM64_TRIPLE/release/lib${RUST_LIB_BASENAME}.dylib"
IOS_SIM_FAT_LIB="$TMP_DIR/lib${FRAMEWORK_NAME}-sim.dylib"

if [ "$SIM_X64_BUILT" -eq 1 ]; then
  IOS_SIM_X64_LIB="$TURSO_DIR/target/$IOS_SIM_X64_TRIPLE/release/lib${RUST_LIB_BASENAME}.dylib"
  lipo -create "$IOS_SIM_ARM64_LIB" "$IOS_SIM_X64_LIB" -output "$IOS_SIM_FAT_LIB"
else
  cp "$IOS_SIM_ARM64_LIB" "$IOS_SIM_FAT_LIB"
fi
make_framework "$SIM_SLICE_DIR" "$IOS_SIM_FAT_LIB"

xcodebuild -create-xcframework \
  -framework "$DEVICE_SLICE_DIR/${FRAMEWORK_NAME}.framework" \
  -framework "$SIM_SLICE_DIR/${FRAMEWORK_NAME}.framework" \
  -output "$IOS_TARGET_DIR"

echo "[op-sqlite] Installing Android .so artifacts"
for triple in "${ANDROID_TRIPLES[@]}"; do
  abi="$(android_abi_from_triple "$triple")"
  mkdir -p "$ANDROID_TARGET_DIR/$abi"
  cp "$TURSO_DIR/target/$triple/release/lib${RUST_LIB_BASENAME}.so" "$ANDROID_TARGET_DIR/$abi/libturso_sdk_kit.so"
done

cp "$TURSO_DIR/sdk-kit/turso.h" "$ANDROID_INCLUDE_DIR/turso.h"
if [ -f "$SYNC_HEADER_SRC" ]; then
  cp "$SYNC_HEADER_SRC" "$ANDROID_INCLUDE_DIR/turso_sync.h"
fi

echo "[op-sqlite] Validating Turso sync SDK availability"

if [ ! -f "$SYNC_HEADER_SRC" ]; then
  echo "[op-sqlite] Missing required sync header: $SYNC_HEADER_SRC"
  exit 1
fi

for triple in "${ANDROID_TRIPLES[@]}"; do
  abi="$(android_abi_from_triple "$triple")"
  so_file="$ANDROID_TARGET_DIR/$abi/libturso_sdk_kit.so"

  if ! nm -D "$so_file" | grep -q "turso_sync_database_new"; then
    echo "[op-sqlite] Missing turso_sync_database_new in $so_file"
    exit 1
  fi

  if ! nm -D "$so_file" | grep -q "turso_sync_database_connect"; then
    echo "[op-sqlite] Missing turso_sync_database_connect in $so_file"
    exit 1
  fi

  if ! nm -D "$so_file" | grep -q "turso_sync_operation_resume"; then
    echo "[op-sqlite] Missing turso_sync_operation_resume in $so_file"
    exit 1
  fi
done

echo "[op-sqlite] Turso binaries installed:"
echo "  - iOS: $IOS_TARGET_DIR"
echo "  - Android: $ANDROID_TARGET_DIR"
echo "  - Sync validation: enabled"