#!/usr/bin/env bash
set -euo pipefail

WEB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PYTHON_BIN="${PYTHON:-$(command -v python3 || command -v python || true)}"
if [ -z "$PYTHON_BIN" ]; then
  echo "Could not find python3 or python in PATH." >&2
  exit 1
fi

if [ "$#" -eq 0 ]; then
  "$PYTHON_BIN" "$WEB_DIR/sarcasmos.py" --start --all --ngrok --keepalive
else
  "$PYTHON_BIN" "$WEB_DIR/sarcasmos.py" "$@"
fi
