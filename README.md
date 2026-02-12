# Light3D

A minimal, lightweight 3D engine inspired by Three.js, built with modern C++17/C++20 and designed for cross-platform development.

## Features

- **Modern C++**: Built with C++17, with optional C++20 support for coroutines
- **Cross-Platform**: Supports Windows, macOS, Linux, and Web (WASM)
- **Multiple Rendering Backends**:
  - OpenGL (desktop platforms)
  - Metal (macOS/iOS)
  - Vulkan (desktop platforms)
  - WebGL/WebGPU (web/WASM)
- **CMake Build System**: Uses Ninja as the default generator
- **Minimal Dependencies**: Lightweight and easy to integrate
- **WASM Support**: Compile to WebAssembly for browser deployment

## Prerequisites

### All Platforms
- CMake 3.15 or higher
- Ninja build system
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)

### Platform-Specific Requirements

**Linux:**
```bash
sudo apt-get install cmake ninja-build build-essential libgl1-mesa-dev
```

**macOS:**
```bash
brew install cmake ninja
# Xcode Command Line Tools required for Metal support
xcode-select --install
```

**Windows:**
- Visual Studio 2017 or higher
- CMake and Ninja (can be installed via Visual Studio installer or separately)

**WebAssembly:**
```bash
# Install Emscripten SDK
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh  # Linux/macOS
# or emsdk_env.bat on Windows
```

## Building

### Native Build (Linux/macOS/Windows)

**Using build scripts:**
```bash
# Linux/macOS
./build_native.sh         # Release build (default)
./build_native.sh Debug   # Debug build

# Windows
build_native.bat          # Release build (default)
build_native.bat Debug    # Debug build
```

**Using CMake Presets (CMake 3.19+):**
```bash
# List available presets
cmake --list-presets

# Configure and build with a preset
cmake --preset release     # Standard release build
cmake --preset debug       # Debug build
cmake --preset cpp20       # C++20 with coroutines
cmake --preset vulkan      # With Vulkan support

# Build
cmake --build build/release
```

**Manual CMake configuration:**
```bash
mkdir build && cd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
ninja
```

### WebAssembly Build

```bash
# Make sure Emscripten is activated first
source /path/to/emsdk/emsdk_env.sh

# Build for WASM
./build_wasm.sh

# Test in browser
cd build_wasm/examples
python3 -m http.server 8080
# Open http://localhost:8080/simple_example.html
```

## CMake Options

- `LIGHT3D_BUILD_EXAMPLES` (default: ON) - Build example applications
- `LIGHT3D_BUILD_TESTS` (default: OFF) - Build tests
- `LIGHT3D_ENABLE_CPP20` (default: OFF) - Enable C++20 features including coroutines
- `LIGHT3D_ENABLE_OPENGL` (default: ON) - Enable OpenGL backend
- `LIGHT3D_ENABLE_METAL` (default: OFF, ON for macOS) - Enable Metal backend
- `LIGHT3D_ENABLE_VULKAN` (default: OFF) - Enable Vulkan backend
- `LIGHT3D_ENABLE_WEBGPU` (default: OFF, ON for WASM) - Enable WebGPU backend

### Example: Building with C++20 and Vulkan

```bash
mkdir build && cd build
cmake -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DLIGHT3D_ENABLE_CPP20=ON \
  -DLIGHT3D_ENABLE_VULKAN=ON \
  ..
ninja
```

## Quick Start

```cpp
#include "light3d/light3d.h"

int main() {
    // Configure engine
    light3d::EngineConfig config;
    config.backend = light3d::RenderBackend::OpenGL;
    config.width = 800;
    config.height = 600;
    
    // Create and initialize engine
    auto engine = light3d::Engine::create(config.backend);
    if (!engine->initialize(config)) {
        return 1;
    }
    
    // Main loop
    while (engine->isRunning()) {
        engine->update(deltaTime);
        engine->render();
    }
    
    engine->shutdown();
    return 0;
}
```

## Project Structure

```
light3d/
├── CMakeLists.txt          # Root CMake configuration
├── include/light3d/        # Public headers
│   └── light3d.h          # Main header file
├── src/                    # Implementation files
│   ├── CMakeLists.txt     # Library CMake config
│   ├── engine.cpp         # Engine implementation
│   ├── renderer_opengl.cpp
│   ├── renderer_webgl.cpp
│   └── ...
├── examples/               # Example applications
│   ├── simple_example.cpp
│   └── shell.html         # WASM HTML template
├── cmake/                  # CMake modules and toolchains
│   └── emscripten.toolchain.cmake
├── build_native.sh         # Native build script
└── build_wasm.sh          # WASM build script
```

## Running Examples

### Native Example
```bash
./build_native.sh
./build/examples/simple_example
```

### WASM Example
```bash
./build_wasm.sh
cd build_wasm/examples
python3 -m http.server 8080
# Open browser to http://localhost:8080/simple_example.html
```

## License

See [LICENSE](LICENSE) file for details.

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues.

## Roadmap

- [ ] Enhanced geometry primitives
- [ ] Material system
- [ ] Lighting support
- [ ] Texture loading
- [ ] Model loading (glTF, OBJ)
- [ ] Physics integration
- [ ] Advanced rendering features
- [ ] More examples and documentation
