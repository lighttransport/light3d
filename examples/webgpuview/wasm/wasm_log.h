#pragma once
#include <cstdio>

// Verbose info logging for the WASM bindings. Off by default (Release); the
// CMake Debug build defines LIGHTUSD_WASM_VERBOSE=1. Errors still use printf().
#ifndef LIGHTUSD_WASM_VERBOSE
#define LIGHTUSD_WASM_VERBOSE 0
#endif

#define LUSD_LOG(...) do { if (LIGHTUSD_WASM_VERBOSE) std::printf(__VA_ARGS__); } while (0)
