#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if ! command -v ffprobe >/dev/null 2>&1; then
  printf 'Error: ffprobe is required but not installed.\n' >&2
  exit 1
fi

shopt -s nullglob
audio_files=(
  "$SCRIPT_DIR"/*.wav
  "$SCRIPT_DIR"/*.mp3
  "$SCRIPT_DIR"/*.flac
  "$SCRIPT_DIR"/*.m4a
  "$SCRIPT_DIR"/*.ogg
  "$SCRIPT_DIR"/*.aac
  "$SCRIPT_DIR"/*.opus
  "$SCRIPT_DIR"/*.wma
)
shopt -u nullglob

if (( ${#audio_files[@]} == 0 )); then
  printf 'No audio files found in %s\n' "$SCRIPT_DIR"
  exit 0
fi

for audio_file in "${audio_files[@]}"; do
  sample_rate="$(ffprobe -v error -select_streams a:0 -show_entries stream=sample_rate -of default=noprint_wrappers=1:nokey=1 "$audio_file")"

  if [[ -n "$sample_rate" ]]; then
    printf '%s: %s Hz\n' "$(basename "$audio_file")" "$sample_rate"
  else
    printf '%s: sample rate not found\n' "$(basename "$audio_file")"
  fi
done
