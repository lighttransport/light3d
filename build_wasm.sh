#!/bin/bash
# Build script for WebAssembly (WASM) using Emscripten

set -e

BUILD_DIR="build_wasm"

echo "Building Light3D for WebAssembly..."

# Check if Emscripten is available
if [ -z "$EMSDK" ]; then
    echo "Error: EMSDK environment variable not set"
    echo "Please install and activate Emscripten SDK:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake using Ninja generator and Emscripten toolchain
emcmake cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=../cmake/emscripten.toolchain.cmake \
    -DLIGHT3D_BUILD_EXAMPLES=ON \
    -DLIGHT3D_ENABLE_WEBGPU=ON \
    ..

# Build
ninja

echo ""
echo "WASM build complete!"
echo "Output files:"
echo "  - $BUILD_DIR/examples/simple_example.html"
echo "  - $BUILD_DIR/examples/simple_example.js"
echo "  - $BUILD_DIR/examples/simple_example.wasm"
echo ""
echo "To test, run a local web server in the build directory:"
echo "  cd $BUILD_DIR/examples"
echo "  python3 -m http.server 8080"
echo "Then open http://localhost:8080/simple_example.html in your browser"
