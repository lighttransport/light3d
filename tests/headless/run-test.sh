#!/usr/bin/env bash
# All-in-one: launch Chrome under xvfb, run connection tests, then shut down.
# Usage:
#   ./run-test.sh [--swiftshader]

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PORT="${CDP_PORT:-9222}"
CHROME_PID=""

cleanup() {
  if [[ -n "$CHROME_PID" ]]; then
    echo "Shutting down Chrome (pid=$CHROME_PID)..."
    kill "$CHROME_PID" 2>/dev/null || true
    wait "$CHROME_PID" 2>/dev/null || true
  fi
}
trap cleanup EXIT

# Launch Chrome in background (suppress Chrome stderr noise)
echo "Starting Chrome..."
"$SCRIPT_DIR/launch-chrome.sh" "$@" 2>/tmp/chrome-headless-stderr.log &
CHROME_PID=$!

# Wait for CDP to become available (up to 15 seconds)
echo "Waiting for CDP on port $PORT..."
for i in $(seq 1 30); do
  if curl -sf "http://127.0.0.1:$PORT/json/version" > /dev/null 2>&1; then
    echo "CDP ready after ~$((i / 2))s"
    break
  fi
  if ! kill -0 "$CHROME_PID" 2>/dev/null; then
    echo "ERROR: Chrome process exited unexpectedly"
    exit 1
  fi
  sleep 0.5
done

if ! curl -sf "http://127.0.0.1:$PORT/json/version" > /dev/null 2>&1; then
  echo "ERROR: CDP did not become available within 15s"
  exit 1
fi

# Run tests
"$SCRIPT_DIR/test-mcp-connection.sh"
