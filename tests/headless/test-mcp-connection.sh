#!/usr/bin/env bash
# Test that Chrome is reachable via CDP and that WebGL2/WebGPU are available.
# Prerequisites: launch-chrome.sh must be running.
#
# Usage: ./test-mcp-connection.sh

set -uo pipefail

PORT="${CDP_PORT:-9222}"
CDP_URL="http://127.0.0.1:$PORT"
PASS=0
FAIL=0

pass() { echo "  PASS: $1"; PASS=$((PASS + 1)); }
fail() { echo "  FAIL: $1"; FAIL=$((FAIL + 1)); }

echo "=== Chrome DevTools Protocol connection test ==="
echo "Target: $CDP_URL"
echo ""

# ---- 1. CDP /json/version reachable ----
echo "[1] CDP endpoint reachable"
if VERSION_JSON=$(curl -sf "$CDP_URL/json/version"); then
  pass "/json/version responded"
  BROWSER=$(echo "$VERSION_JSON" | python3 -c 'import sys,json; print(json.load(sys.stdin).get("Browser","?"))' 2>/dev/null || echo '?')
  echo "    Browser: $BROWSER"
else
  fail "/json/version unreachable — is Chrome running with --remote-debugging-port=$PORT?"
fi

# ---- 2. Page targets & get WS URL ----
echo ""
echo "[2] Page targets"
PAGE_WS=""
if TARGETS=$(curl -sf "$CDP_URL/json/list"); then
  # Find the first 'page' type target
  PAGE_WS=$(echo "$TARGETS" | python3 -c '
import sys, json
targets = json.load(sys.stdin)
pages = [t for t in targets if t.get("type") == "page"]
print(pages[0]["webSocketDebuggerUrl"] if pages else "")
' 2>/dev/null || true)
  PAGE_COUNT=$(echo "$TARGETS" | python3 -c 'import sys,json; print(len([t for t in json.load(sys.stdin) if t.get("type")=="page"]))' 2>/dev/null || echo 0)
  if [[ "$PAGE_COUNT" -gt 0 ]]; then
    pass "$PAGE_COUNT page target(s) found"
  else
    fail "no page targets found"
  fi
else
  fail "/json/list unreachable"
fi

# ---- Helper: evaluate JS expression via CDP on existing page ----
cdp_eval() {
  local js_file="$1"
  local await_promise="${2:-false}"

  if [[ -z "${PAGE_WS:-}" ]]; then
    echo "NO_PAGE"
    return
  fi

  node --input-type=module -e "
import { readFileSync } from 'fs';
const expr = readFileSync('${js_file}', 'utf-8');
const ws = new WebSocket('${PAGE_WS}');
ws.onopen = () => {
  ws.send(JSON.stringify({
    id: 1,
    method: 'Runtime.evaluate',
    params: { expression: expr, returnByValue: true, awaitPromise: ${await_promise} }
  }));
};
ws.onmessage = (ev) => {
  const msg = JSON.parse(ev.data);
  if (msg.id === 1) {
    const val = msg.result?.result?.value;
    console.log(val !== undefined ? val : JSON.stringify(msg.result));
    ws.close();
    process.exit(0);
  }
};
ws.onerror = () => { console.log('WS_ERROR'); process.exit(1); };
setTimeout(() => { console.log('TIMEOUT'); process.exit(1); }, 15000);
" 2>/dev/null || echo "NODE_ERROR"
}

# ---- 3. WebGL2 support ----
echo ""
echo "[3] WebGL2 support"

TMPJS=$(mktemp /tmp/cdp-webgl2-XXXXXX.js)
cat > "$TMPJS" <<'JSEOF'
(() => {
  const c = document.createElement("canvas");
  const gl = c.getContext("webgl2");
  if (gl) {
    const r = gl.getExtension("WEBGL_debug_renderer_info");
    return JSON.stringify({
      supported: true,
      vendor: r ? gl.getParameter(r.UNMASKED_VENDOR_WEBGL) : gl.getParameter(gl.VENDOR),
      renderer: r ? gl.getParameter(r.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER)
    });
  } else {
    return JSON.stringify({supported: false});
  }
})()
JSEOF

RESULT=$(cdp_eval "$TMPJS" "false")
rm -f "$TMPJS"

if echo "$RESULT" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); exit(0 if d.get("supported") else 1)' 2>/dev/null; then
  RENDERER=$(echo "$RESULT" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d.get("renderer","?"))' 2>/dev/null || echo "?")
  pass "WebGL2 available (renderer: $RENDERER)"
else
  fail "WebGL2 not available (result: $RESULT)"
fi

# ---- 4. WebGPU support ----
echo ""
echo "[4] WebGPU support"
# WebGPU requires a secure context. Navigate to http://localhost which is
# always considered secure by browsers, then evaluate the check.

TMPJS=$(mktemp /tmp/cdp-webgpu-XXXXXX.js)
cat > "$TMPJS" <<'JSEOF'
(async () => {
  if (!navigator.gpu) return JSON.stringify({supported: false, reason: "no navigator.gpu"});
  const adapter = await navigator.gpu.requestAdapter();
  if (!adapter) return JSON.stringify({supported: false, reason: "no adapter"});
  const info = adapter.info || {};
  return JSON.stringify({supported: true, vendor: info.vendor || "?", architecture: info.architecture || "?", description: info.description || "?"});
})()
JSEOF

# Navigate the page to localhost (CDP endpoint itself) to get a secure context
if [[ -n "${PAGE_WS:-}" ]]; then
  node --input-type=module -e "
const ws = new WebSocket('${PAGE_WS}');
ws.onopen = () => {
  ws.send(JSON.stringify({id:1, method:'Page.navigate', params:{url:'http://localhost:${PORT}/json/version'}}));
};
ws.onmessage = (ev) => {
  const msg = JSON.parse(ev.data);
  if (msg.id === 1) { ws.close(); process.exit(0); }
};
setTimeout(() => process.exit(0), 5000);
" 2>/dev/null || true
  sleep 1
fi

RESULT=$(cdp_eval "$TMPJS" "true")
rm -f "$TMPJS"

if echo "$RESULT" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); exit(0 if d.get("supported") else 1)' 2>/dev/null; then
  DESC=$(echo "$RESULT" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d.get("description","?"))' 2>/dev/null || echo "?")
  pass "WebGPU available (description: $DESC)"
else
  REASON=$(echo "$RESULT" | python3 -c 'import sys,json; d=json.loads(sys.stdin.read()); print(d.get("reason","unknown"))' 2>/dev/null || echo "unknown")
  fail "WebGPU not available ($REASON)"
fi

# ---- Summary ----
echo ""
echo "=== Results: $PASS passed, $FAIL failed ==="
exit "$FAIL"
