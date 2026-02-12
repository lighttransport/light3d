# Contributing to Light3D

Thank you for your interest in contributing to Light3D! This document provides guidelines and information for contributors.

## Development Setup

### Prerequisites

Ensure you have the following installed:
- CMake 3.15 or higher
- Ninja build system
- C++17 or C++20 compatible compiler
- Git

### Building from Source

1. Clone the repository:
```bash
git clone https://github.com/lighttransport/light3d.git
cd light3d
```

2. Build the project:
```bash
./build_native.sh       # Linux/macOS
# or
build_native.bat        # Windows
```

## Project Structure

```
light3d/
├── include/light3d/    # Public API headers
├── src/                # Implementation files
├── examples/           # Example applications
├── cmake/              # CMake modules and toolchains
└── tests/              # Test files (when implemented)
```

## Coding Standards

### C++ Style
- Use C++17 as the baseline (C++20 for optional features)
- Follow modern C++ best practices
- Use `snake_case` for variables and functions
- Use `PascalCase` for classes and structs
- Use meaningful names that describe purpose
- Keep functions focused and concise

### Code Organization
- Header files go in `include/light3d/`
- Implementation files go in `src/`
- Platform-specific code should be clearly marked with `#ifdef` guards
- Backend-specific implementations should be in separate files

### Comments
- Use clear, concise comments for complex logic
- Document public APIs with descriptive comments
- Prefer self-documenting code where possible

## Building for Different Platforms

### Native Platforms
```bash
# Release build
cmake --preset release && cmake --build build/release

# Debug build
cmake --preset debug && cmake --build build/debug

# C++20 with coroutines
cmake --preset cpp20 && cmake --build build/cpp20
```

### WebAssembly
```bash
# Ensure Emscripten is activated
source /path/to/emsdk/emsdk_env.sh

# Build
./build_wasm.sh
```

## Adding New Features

### Backend Support
When adding a new rendering backend:
1. Create a new renderer implementation file (e.g., `renderer_dx12.cpp`)
2. Add the backend enum to `include/light3d/light3d.h`
3. Update CMake configuration in `src/CMakeLists.txt`
4. Add platform detection and initialization in `src/engine.cpp`
5. Update documentation

### Platform Support
When adding support for a new platform:
1. Add platform detection macros in `light3d.h`
2. Create platform-specific code with appropriate `#ifdef` guards
3. Update CMake configuration
4. Add build instructions to README
5. Test on the target platform

## Testing

Currently, Light3D does not have a comprehensive test suite. When adding new features:
- Ensure example programs still build and run
- Test on multiple platforms when possible
- Document any platform-specific behavior

## Pull Request Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Ensure code builds on target platforms
5. Commit your changes (`git commit -m 'Add amazing feature'`)
6. Push to the branch (`git push origin feature/amazing-feature`)
7. Open a Pull Request

### PR Guidelines
- Provide a clear description of changes
- Reference any related issues
- Keep changes focused and minimal
- Ensure CI builds pass (when available)
- Update documentation as needed

## Reporting Issues

When reporting issues, please include:
- Operating system and version
- Compiler and version
- CMake version
- Build configuration used
- Steps to reproduce
- Expected vs actual behavior
- Error messages or logs

## License

By contributing to Light3D, you agree that your contributions will be licensed under the same license as the project (see LICENSE file).

## Questions?

Feel free to open an issue for questions or discussions about:
- Feature requests
- Design decisions
- Implementation approaches
- Platform support

Thank you for contributing to Light3D!
