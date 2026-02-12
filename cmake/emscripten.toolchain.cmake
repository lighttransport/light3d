# Emscripten toolchain file for WebAssembly builds

set(CMAKE_SYSTEM_NAME Emscripten)
set(CMAKE_SYSTEM_VERSION 1)

# Find Emscripten compiler
if(NOT EMSCRIPTEN_ROOT)
    if(DEFINED ENV{EMSDK})
        set(EMSCRIPTEN_ROOT "$ENV{EMSDK}/upstream/emscripten")
    else()
        message(FATAL_ERROR "EMSCRIPTEN_ROOT or EMSDK environment variable not set")
    endif()
endif()

set(CMAKE_C_COMPILER "${EMSCRIPTEN_ROOT}/emcc")
set(CMAKE_CXX_COMPILER "${EMSCRIPTEN_ROOT}/em++")
set(CMAKE_AR "${EMSCRIPTEN_ROOT}/emar" CACHE FILEPATH "Emscripten ar")
set(CMAKE_RANLIB "${EMSCRIPTEN_ROOT}/emranlib" CACHE FILEPATH "Emscripten ranlib")

# Set compiler flags
set(CMAKE_C_FLAGS_INIT "-s USE_WEBGL2=1")
set(CMAKE_CXX_FLAGS_INIT "-s USE_WEBGL2=1")

# Set executable suffix
set(CMAKE_EXECUTABLE_SUFFIX ".js")

# Adjust the default behavior of the FIND_XXX() commands
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
