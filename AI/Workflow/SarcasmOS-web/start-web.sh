#!/usr/bin/env bash
set -euo pipefail

WEB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "Starting SarcasmOS web on http://localhost:5173"
echo "Press Ctrl+C to stop."
python -m http.server 5173 -d "$WEB_DIR"
