# Testing

## Headless WebGPU testing via Firefox devtools MCP

The `examples/webgpuview` viewer, the file-neutral harness, and the WebGPU
feature-test suite are driven headlessly through the **Firefox devtools MCP**
server (`@mozilla/firefox-devtools-mcp`). It launches a headless Firefox,
navigates pages, evaluates JavaScript, and captures screenshots — enough to load
USD/glTF models and assert that skinning / blendshape / PBR actually render.

### `.mcp.json` setup

`.mcp.json` is **not committed** because it contains a machine-specific Firefox
path. Create/edit it in the repo root with the block below, replacing
`--firefox-path` with your own Firefox binary:

```json
{
  "mcpServers": {
    "firefox-devtools": {
      "type": "stdio",
      "command": "npx",
      "args": [
        "@mozilla/firefox-devtools-mcp@latest",
        "--firefox-path", "/home/you/local/firefox/firefox",
        "--headless",
        "--viewport", "1280x720",
        "--enableScript"
      ],
      "env": {}
    }
  }
}
```

Flag notes:

| Flag | Purpose |
|------|---------|
| `--firefox-path` | Path to the Firefox binary. **Use Firefox 153+** — `--enableScript` requires it. Omit to use the system Firefox (only if it is 153+). |
| `--headless` | No window; required for CI / remote sessions. |
| `--viewport WxH` | Backing size for screenshots (`1280x720`). Max `3840x2160` in headless. |
| `--enableScript` | Exposes the `evaluate_script` / logpoint tools. **Required** to query `navigator.gpu`, call `window.loadGltfFromMemory`, `window.__renderOffscreen`, etc. Needs Firefox 153+. |

After creating or editing `.mcp.json`, **reconnect the server** so the new args
take effect — run `/mcp` and reconnect `firefox-devtools` (or restart the
session). Editing the file does not hot-reload an already-running MCP server.

> WebGPU on Linux/Firefox may be behind a pref. If `navigator.gpu` is missing,
> set `dom.webgpu.enabled=true` (e.g. via `restart_firefox` with
> `prefs: { "dom.webgpu.enabled": true }`, which needs
> `MOZ_REMOTE_ALLOW_SYSTEM_ACCESS=1`).

### Verifying GPU hardware acceleration

Headless Firefox can silently fall back to the `llvmpipe` software renderer.
To confirm the real GPU is used, restart Firefox with wgpu logging and read the
adapter the backend selected:

```
restart_firefox env=["MOZ_LOG=WebGPU:5,wgpu:5,wgpu_hal:5", "RUST_LOG=wgpu_hal=info,wgpu_core=info"]
get_firefox_output grep="adapter"
```

A hardware result looks like:

```
Request adapter result AdapterInfo { name: "NVIDIA GeForce RTX 3070",
  device_type: DiscreteGpu, backend: Vulkan, driver: "NVIDIA", ... }
```

If `name` is `llvmpipe` the run is software-only.

### Running the pages under test

Serve `examples/webgpuview` over HTTP (WebGPU needs `http://`, not `file://`),
then navigate the MCP-driven Firefox to the page:

```bash
cd examples/webgpuview
npm install && npm run dev          # http://localhost:8000/  (or: python3 -m http.server 8000)
```

| Page | Purpose | Headless verification hook |
|------|---------|----------------------------|
| `tests/webgpu-feature-tests.html` | WebGPU capability + correctness suite | `await window.runWebGPUTests()` → JSON; `window.__webgpuTestsDone` |
| `harness.html` | glTF skinning / morph / PBR + animation | `window.__renderOffscreen(W,H,morphW)` (coverage/centroid), `window.__seekAnim(t)`, `window.__renderOnce()` |
| `index.html` | USD viewer | `window.loadUsdBuffer(name, arrayBuffer)` |

> Headless Firefox throttles `requestAnimationFrame`, so for deterministic
> checks render on demand via `window.__renderOnce()` / `window.__renderOffscreen()`
> rather than relying on the rAF loop.

### `chrome-devtools` (optional)

A `chrome-devtools` MCP entry can be added the same way for Chrome-based testing;
it attaches to a Chrome started with `--remote-debugging-port=9222`:

```json
"chrome-devtools": {
  "command": "npx",
  "args": ["-y", "chrome-devtools-mcp@latest", "--browserUrl=http://127.0.0.1:9222", "--no-usage-statistics"]
}
```
