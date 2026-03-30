#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="${1:-"$ROOT_DIR/cpp"}"
DOWNLOAD_PAGE_URL="https://www.sqlite.org/download.html"
SQLITE_BASE_URL="https://www.sqlite.org"

if ! command -v curl >/dev/null 2>&1; then
  echo "Error: curl is required but not installed." >&2
  exit 1
fi

if ! command -v unzip >/dev/null 2>&1; then
  echo "Error: unzip is required but not installed." >&2
  exit 1
fi

mkdir -p "$TARGET_DIR"

TMP_DIR="$(mktemp -d)"
cleanup() {
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

echo "Fetching SQLite download page..."
PAGE_CONTENT="$(curl -fsSL "$DOWNLOAD_PAGE_URL")"

ZIP_PATH="$(printf '%s' "$PAGE_CONTENT" | grep -Eo '[0-9]{4}/sqlite-amalgamation-[0-9]+\.zip' | head -n 1 || true)"
if [[ -z "$ZIP_PATH" ]]; then
  ZIP_PATH="$(printf '%s' "$PAGE_CONTENT" | grep -Eo 'sqlite-amalgamation-[0-9]+\.zip' | head -n 1 || true)"
fi

if [[ -z "$ZIP_PATH" ]]; then
  echo "Error: Could not find a sqlite-amalgamation zip link on $DOWNLOAD_PAGE_URL." >&2
  exit 1
fi

if [[ "$ZIP_PATH" == http* ]]; then
  ZIP_URL="$ZIP_PATH"
else
  ZIP_URL="$SQLITE_BASE_URL/$ZIP_PATH"
fi

echo "Downloading: $ZIP_URL"
ZIP_FILE="$TMP_DIR/sqlite-amalgamation.zip"
curl -fsSL "$ZIP_URL" -o "$ZIP_FILE"

unzip -q "$ZIP_FILE" -d "$TMP_DIR"

AMALGAMATION_DIR="$(find "$TMP_DIR" -maxdepth 1 -type d -name 'sqlite-amalgamation-*' | head -n 1 || true)"
if [[ -z "$AMALGAMATION_DIR" ]]; then
  echo "Error: sqlite-amalgamation directory not found in archive." >&2
  exit 1
fi

if [[ ! -f "$AMALGAMATION_DIR/sqlite3.c" || ! -f "$AMALGAMATION_DIR/sqlite3.h" ]]; then
  echo "Error: sqlite3.c/sqlite3.h not found in extracted amalgamation." >&2
  exit 1
fi

cp "$AMALGAMATION_DIR/sqlite3.c" "$TARGET_DIR/sqlite3.c"
cp "$AMALGAMATION_DIR/sqlite3.h" "$TARGET_DIR/sqlite3.h"

VERSION="$(basename "$AMALGAMATION_DIR" | sed -E 's/^sqlite-amalgamation-([0-9]+)$/\1/')"

echo "SQLite amalgamation updated successfully."
echo "Version tag: $VERSION"
echo "Updated files:"
echo "  - $TARGET_DIR/sqlite3.c"
echo "  - $TARGET_DIR/sqlite3.h"
