#!/usr/bin/env bash
set -euo pipefail

WEB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$WEB_DIR/../../.." && pwd)"
VENV_DIR="$ROOT_DIR/.venv"
PYTHON_BIN="$VENV_DIR/bin/python"
BACKEND_ENV="$WEB_DIR/backend/.env"

if [ ! -d "$VENV_DIR" ]; then
  python -m venv "$VENV_DIR"
fi

"$PYTHON_BIN" -m pip install -r "$WEB_DIR/backend/requirements.txt"
if ! "$PYTHON_BIN" -m pip check >/dev/null; then
  "$PYTHON_BIN" -m pip install --ignore-installed -r "$WEB_DIR/backend/requirements.txt"
fi

if [ ! -f "$BACKEND_ENV" ]; then
  cp "$WEB_DIR/backend/.env.example" "$BACKEND_ENV"
  echo "Created backend/.env from backend/.env.example. Add your API keys before using chat/TTS."
fi

cd "$WEB_DIR"
echo "Starting backend on http://localhost:8001"
"$PYTHON_BIN" -m uvicorn backend.app:app --host 0.0.0.0 --port 8001 &
BACKEND_PID=$!

cleanup() {
  kill "$BACKEND_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "Starting web on http://localhost:5173"
"$PYTHON_BIN" -m http.server 5173 -d "$WEB_DIR"
