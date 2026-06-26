#!/usr/bin/env bash
# Build lightusd-c WASM module for webgpuview.
# Usage: ./build.sh [--debug]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
BUILD_TYPE="Release"

if [[ "${1:-}" == "--debug" ]]; then
  BUILD_TYPE="Debug"
fi

echo "Building lightusd WASM ($BUILD_TYPE)..."

emcmake cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
  -G "Unix Makefiles"

cmake --build "$BUILD_DIR" --target lightusd_wasm -j$(nproc) 2>&1

# Copy outputs to parent directory (next to index.html)
cp "$BUILD_DIR/lightusd.js"   "$SCRIPT_DIR/../lightusd.js"
cp "$BUILD_DIR/lightusd.wasm" "$SCRIPT_DIR/../lightusd.wasm"

echo "Output: $SCRIPT_DIR/../lightusd.js + lightusd.wasm"
