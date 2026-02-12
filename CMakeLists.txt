cmake_minimum_required(VERSION 3.15)

project(light3d 
    VERSION 0.1.0
    DESCRIPTION "Lightweight 3D engine supporting OpenGL/Metal/Vulkan and WebGL/WebGPU"
    LANGUAGES CXX
)

# Set default build type
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

# C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(LIGHT3D_BUILD_EXAMPLES "Build examples" ON)
option(LIGHT3D_BUILD_TESTS "Build tests" OFF)
option(LIGHT3D_ENABLE_CPP20 "Enable C++20 features (coroutines)" OFF)
option(LIGHT3D_ENABLE_OPENGL "Enable OpenGL backend" ON)
option(LIGHT3D_ENABLE_METAL "Enable Metal backend (macOS/iOS)" OFF)
option(LIGHT3D_ENABLE_VULKAN "Enable Vulkan backend" OFF)
option(LIGHT3D_ENABLE_WEBGPU "Enable WebGPU backend (WASM)" OFF)

# Detect Emscripten
if(EMSCRIPTEN)
    message(STATUS "Building for WASM with Emscripten")
    set(LIGHT3D_ENABLE_OPENGL OFF)
    set(LIGHT3D_ENABLE_WEBGPU ON)
    set(LIGHT3D_PLATFORM "WASM")
elseif(APPLE)
    set(LIGHT3D_PLATFORM "macOS")
    set(LIGHT3D_ENABLE_METAL ON)
elseif(WIN32)
    set(LIGHT3D_PLATFORM "Windows")
elseif(UNIX)
    set(LIGHT3D_PLATFORM "Linux")
endif()

# Enable C++20 if requested
if(LIGHT3D_ENABLE_CPP20)
    set(CMAKE_CXX_STANDARD 20)
    message(STATUS "C++20 enabled with coroutine support")
endif()

message(STATUS "Light3D Configuration:")
message(STATUS "  Platform: ${LIGHT3D_PLATFORM}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  OpenGL: ${LIGHT3D_ENABLE_OPENGL}")
message(STATUS "  Metal: ${LIGHT3D_ENABLE_METAL}")
message(STATUS "  Vulkan: ${LIGHT3D_ENABLE_VULKAN}")
message(STATUS "  WebGPU: ${LIGHT3D_ENABLE_WEBGPU}")

# Add subdirectories
add_subdirectory(src)

if(LIGHT3D_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(LIGHT3D_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
