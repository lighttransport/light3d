# webgpuview — light3d WebGPU viewer & harness

WebGPU viewer plus a file-format-neutral skinning / blendshape / PBR harness.
USD is loaded via `lightusd-c`, glTF 2.0 via **tinygltf v3** — both compiled to WASM.

Pages:
- `index.html` — USD viewer (drag-drop `.usd/.usdc/.usda`).
- `harness.html` — neutral glTF harness: skinning + morph (default-on) + PBR, with
  skeletal-animation playback. Keys: `1/2/3` load sample models, drag-drop a `.glb/.gltf`,
  `P` pause animation, `M` toggle morph animation, drag to orbit, scroll to zoom.
- `tests/webgpu-feature-tests.html` — WebGPU capability + correctness suite.

Needs a WebGPU browser (Firefox 141+/Chrome 113+/Safari 18+). On Linux/Firefox you may need
`dom.webgpu.enabled` in `about:config`.

## Run with npm / Vite (recommended)

```bash
npm install            # one-time
npm run dev            # dev server on http://localhost:8000  (open /harness.html)
npm run build          # production build → dist/
npm run preview        # serve the built dist/ on :8000
```

`lightusd.js` + `lightusd.wasm` and the `models/` assets are loaded at runtime; the Vite
config (`vite.config.js`) serves them in dev and copies them into `dist/` on build.

## Run without npm

Any static server works (WebGPU needs http://, not file://):

```bash
python3 -m http.server 8000     # then open http://localhost:8000/harness.html
```

## Rebuild the WASM

Only needed when `wasm/binding.cc` or `wasm/gltf_binding.cc` change:

```bash
cd wasm
source /path/to/emsdk/emsdk_env.sh
./build.sh                       # writes ../lightusd.js + ../lightusd.wasm
```
