# Headless Chrome + MCP DevTools Test

Tests WebGL2/WebGPU availability in headless Chrome and validates CDP connectivity
for the `chrome-devtools-mcp` server.

## Prerequisites

- Google Chrome 113+
- xvfb (`apt install xvfb`)
- Node.js 22+ (for built-in WebSocket)
- NVIDIA GPU + Vulkan drivers (for HW rendering), or use `--swiftshader`

## Quick start

```bash
# HW GPU (Vulkan/ANGLE on NVIDIA)
./run-test.sh

# Software rendering (no GPU needed)
./run-test.sh --swiftshader
```

## Using with chrome-devtools-mcp

1. Start Chrome manually:
   ```bash
   ./launch-chrome.sh
   ```

2. In another terminal, use Claude Code with the MCP server pointing at the running Chrome:
   ```bash
   claude --mcp-config mcp-chrome.json
   ```

   Or add to project `.mcp.json` (see below).

## MCP configuration

See `mcp-chrome.json` for the project-level configuration that connects
`chrome-devtools-mcp` to the Chrome instance launched by `launch-chrome.sh`.
