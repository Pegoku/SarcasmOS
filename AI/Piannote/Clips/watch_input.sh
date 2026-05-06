#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INPUT_DIR="$SCRIPT_DIR/input"
POLL_SECONDS="${POLL_SECONDS:-1}"

mkdir -p "$INPUT_DIR"

next_clip_number() {
  local highest=0
  local path

  shopt -s nullglob
  for path in "$SCRIPT_DIR"/bender-voz*.wav; do
    if [[ $(basename "$path") =~ ^bender-voz([0-9]+)\.wav$ ]]; then
      local number="${BASH_REMATCH[1]}"
      if (( number > highest )); then
        highest=$number
      fi
    fi
  done
  shopt -u nullglob

  printf '%d\n' "$((highest + 1))"
}

while true; do
  shopt -s nullglob
  for source in "$INPUT_DIR"/*; do
    [[ -f "$source" ]] || continue

    next_number="$(next_clip_number)"
    target="$SCRIPT_DIR/bender-voz${next_number}.wav"

    mv "$source" "$target"
    printf 'Moved %s -> %s\n' "$(basename "$source")" "$(basename "$target")"
  done
  shopt -u nullglob

  sleep "$POLL_SECONDS"
done
