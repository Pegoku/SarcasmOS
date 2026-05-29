#!/usr/bin/env bash
set -euo pipefail

WEB_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python "$WEB_DIR/sarcasmos.py" --start --backend "$@"
