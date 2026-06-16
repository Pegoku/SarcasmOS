#!/usr/bin/env bash
set -euo pipefail

WEB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
if [ "$#" -eq 0 ]; then
  python "$WEB_DIR/sarcasmos.py" --start --all --ngrok --keepalive
else
  python "$WEB_DIR/sarcasmos.py" "$@"
fi
