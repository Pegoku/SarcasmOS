#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$ROOT_DIR/.venv"
BACKEND_ENV="$ROOT_DIR/backend/.env"
PYTHON_BIN="$VENV_DIR/bin/python"

cd "$ROOT_DIR"

if [ ! -d "$VENV_DIR" ]; then
  python -m venv "$VENV_DIR"
fi

"$PYTHON_BIN" -m pip install -r "$ROOT_DIR/backend/requirements.txt"
if ! "$PYTHON_BIN" -m pip check >/dev/null; then
  "$PYTHON_BIN" -m pip install --ignore-installed -r "$ROOT_DIR/backend/requirements.txt"
fi

if [ ! -f "$BACKEND_ENV" ]; then
  cp "$ROOT_DIR/backend/.env.example" "$BACKEND_ENV"
  echo "Created backend/.env from backend/.env.example. Add your API keys before using chat/TTS."
fi

echo "Starting backend on http://localhost:8000"
"$PYTHON_BIN" -m uvicorn backend.app:app --reload --host 0.0.0.0 --port 8000 &
BACKEND_PID=$!

cleanup() {
  kill "$BACKEND_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "Starting frontend on http://localhost:5173"
echo "Open http://localhost:5173 in your browser. Press Ctrl+C to stop."
"$PYTHON_BIN" -m http.server 5173 -d "$ROOT_DIR/AI/Workflow/SarcasmOS-web"
