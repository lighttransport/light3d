#!/usr/bin/env bash
# Launch headless Chrome under xvfb with WebGL2/WebGPU support.
# Usage:
#   ./launch-chrome.sh [--swiftshader]   # default uses HW GPU (Vulkan/ANGLE)
#   ./launch-chrome.sh --swiftshader     # software rendering via SwiftShader
#
# Chrome will listen on --remote-debugging-port=9222 so the
# chrome-devtools-mcp server (or any CDP client) can connect.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${CDP_PORT:-9222}"
USER_DATA_DIR="${CHROME_USER_DATA_DIR:-/tmp/chrome-headless-$$}"

USE_SWIFTSHADER=0
if [[ "${1:-}" == "--swiftshader" ]]; then
  USE_SWIFTSHADER=1
fi

# Common flags
COMMON_FLAGS=(
  --no-sandbox
  --no-first-run
  --disable-background-networking
  --disable-default-apps
  --disable-extensions
  --disable-sync
  --disable-translate
  --metrics-recording-only
  --remote-debugging-port="$PORT"
  --user-data-dir="$USER_DATA_DIR"
)

if [[ "$USE_SWIFTSHADER" -eq 1 ]]; then
  # Software rendering via SwiftShader (no GPU required)
  GPU_FLAGS=(
    --use-webgpu-adapter=swiftshader
    --enable-unsafe-webgpu
    --enable-unsafe-swiftshader
    --enable-webgl
    --disable-gpu-blocklist
    --unsafely-treat-insecure-origin-as-secure="http://localhost"
  )
else
  # Hardware GPU rendering via Vulkan/ANGLE (requires NVIDIA GPU + Vulkan)
  export __NV_PRIME_RENDER_OFFLOAD=1
  export __GLX_VENDOR_LIBRARY_NAME=nvidia
  export __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/10_nvidia.json

  GPU_FLAGS=(
    --use-gl=angle
    --use-angle=vulkan
    --enable-features=Vulkan
    --enable-unsafe-webgpu
    --disable-gpu-blocklist
    --unsafely-treat-insecure-origin-as-secure="http://localhost"
  )
fi

cleanup() {
  echo "Cleaning up user-data-dir: $USER_DATA_DIR"
  rm -rf "$USER_DATA_DIR"
}
trap cleanup EXIT

echo "Launching Chrome (port=$PORT, swiftshader=$USE_SWIFTSHADER)..."
echo "  CDP endpoint: http://127.0.0.1:$PORT"

# xvfb-run is required even for headless — ANGLE/Vulkan init needs an X display.
# Use headless=false under xvfb so Chrome renders to the virtual framebuffer
# (required for WebGPU/WebGL to actually initialize).
exec xvfb-run -a --server-args="-screen 0 1280x720x24" \
  google-chrome-stable \
    "${COMMON_FLAGS[@]}" \
    "${GPU_FLAGS[@]}" \
    about:blank
