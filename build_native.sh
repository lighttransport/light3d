#!/bin/bash
# Build script for native platforms (Linux/macOS/Windows)

set -e

BUILD_TYPE=${1:-Release}
BUILD_DIR="build"

echo "Building Light3D for native platform..."
echo "Build type: $BUILD_TYPE"

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake using Ninja generator
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DLIGHT3D_BUILD_EXAMPLES=ON \
    -DLIGHT3D_ENABLE_OPENGL=ON \
    ..

# Build
ninja

echo ""
echo "Build complete!"
echo "Binary location: $BUILD_DIR/examples/simple_example"
