cmake_minimum_required(VERSION 3.16)
project(lightusd_wasm C CXX)

# --- Paths ---
set(TINYUSDZ_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/tinyusdz")
set(LIGHTUSD_DIR "${TINYUSDZ_DIR}/sandbox/lightusd-c")

# --- Build tinyusdz as static library (subdirectory) ---
set(TINYUSDZ_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINYUSDZ_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(TINYUSDZ_WITH_C_API OFF CACHE BOOL "" FORCE)
set(TINYUSDZ_WITH_PXR_COMPAT_API OFF CACHE BOOL "" FORCE)
set(TINYUSDZ_PRODUCTION_BUILD ON CACHE BOOL "" FORCE)
add_subdirectory("${TINYUSDZ_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/tinyusdz" EXCLUDE_FROM_ALL)

# --- Build lightusd-c (with tinyusdz backend) ---
set(LUSD_TINYUSDZ_DIR "${TINYUSDZ_DIR}" CACHE STRING "" FORCE)
set(LUSD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LUSD_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LYDRA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${LIGHTUSD_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lightusd-c" EXCLUDE_FROM_ALL)

# --- tinygltf v3 (C-first glTF 2.0 loader) ---
set(TINYGLTF_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../deps/tinygltf")

# --- Embind binding module ---
add_executable(lightusd_wasm binding.cc gltf_binding.cc "${TINYGLTF_DIR}/tiny_gltf_v3.c")
target_link_libraries(lightusd_wasm PRIVATE lightusd-c lydra-c)
target_include_directories(lightusd_wasm PRIVATE
  "${LIGHTUSD_DIR}/include"
  "${LIGHTUSD_DIR}/lydra/lydra-c"
  "${TINYGLTF_DIR}"
)
# tiny_gltf_v3.c is C11; image/filesystem I/O stays off (we only read embedded GLB buffers).
set_source_files_properties("${TINYGLTF_DIR}/tiny_gltf_v3.c" PROPERTIES LANGUAGE C)

set_target_properties(lightusd_wasm PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
  OUTPUT_NAME "lightusd"
  SUFFIX ".js"
)

# --- Emscripten link flags ---
target_link_options(lightusd_wasm PRIVATE
  -lembind
  -sENVIRONMENT='web'
  -sMALLOC=emmalloc
  -sALLOW_MEMORY_GROWTH=1
  -sFILESYSTEM=0
  -sMODULARIZE=1
  -sEXPORT_ES6=0
  -sEXPORT_NAME='LightUSD'
  -sINVOKE_RUN=0
  -sWASM_BIGINT=1
  -sSTACK_SIZE=1048576
  -sINITIAL_MEMORY=67108864
  -sEXPORTED_FUNCTIONS=['_malloc','_free']
  -sEXPORTED_RUNTIME_METHODS=['HEAPU8']
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  target_link_options(lightusd_wasm PRIVATE -g -sASSERTIONS=2 -sSAFE_HEAP=1)
  target_compile_definitions(lightusd_wasm PRIVATE LIGHTUSD_WASM_VERBOSE=1)  # verbose binding logs
else()
  target_link_options(lightusd_wasm PRIVATE -Oz -sASSERTIONS=1)
endif()
