#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESIRED_HZ=""
AUTO_CONVERT=0

usage() {
  printf 'Usage: %s [-r HZ|--rate HZ] [-a|--autoconvert]\n' "$(basename "$0")"
}

while (( $# > 0 )); do
  case "$1" in
    -r|--rate)
      if (( $# < 2 )); then
        usage >&2
        exit 1
      fi
      DESIRED_HZ="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    -a|--autoconvert)
      AUTO_CONVERT=1
      shift
      ;;
    *)
      usage >&2
      exit 1
      ;;
  esac
done

if [[ -n "$DESIRED_HZ" && ! "$DESIRED_HZ" =~ ^[0-9]+$ ]]; then
  printf 'Error: desired Hz must be a positive integer.\n' >&2
  exit 1
fi

if (( AUTO_CONVERT == 1 )) && [[ -z "$DESIRED_HZ" ]]; then
  printf 'Error: --autoconvert requires --rate.\n' >&2
  exit 1
fi

if ! command -v ffprobe >/dev/null 2>&1; then
  printf 'Error: ffprobe is required but not installed.\n' >&2
  exit 1
fi

if (( AUTO_CONVERT == 1 )) && ! command -v ffmpeg >/dev/null 2>&1; then
  printf 'Error: ffmpeg is required for auto-convert but not installed.\n' >&2
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
    if [[ -n "$DESIRED_HZ" ]]; then
      if [[ "$sample_rate" == "$DESIRED_HZ" ]]; then
        printf '%s: %s Hz [OK]\n' "$(basename "$audio_file")" "$sample_rate"
      else
        if (( AUTO_CONVERT == 1 )); then
          extension="${audio_file##*.}"
          temp_file="$(mktemp "$SCRIPT_DIR/.check_audio_hz.XXXXXX.${extension}")"

          if ffmpeg -v error -y -i "$audio_file" -ar "$DESIRED_HZ" "$temp_file"; then
            mv "$temp_file" "$audio_file"
            printf '%s: %s Hz -> %s Hz [CONVERTED]\n' "$(basename "$audio_file")" "$sample_rate" "$DESIRED_HZ"
          else
            rm -f "$temp_file"
            printf '%s: %s Hz [FAILED TO CONVERT TO %s]\n' "$(basename "$audio_file")" "$sample_rate" "$DESIRED_HZ" >&2
          fi
        else
          printf '%s: %s Hz [EXPECTED %s]\n' "$(basename "$audio_file")" "$sample_rate" "$DESIRED_HZ"
        fi
      fi
    else
      printf '%s: %s Hz\n' "$(basename "$audio_file")" "$sample_rate"
    fi
  else
    printf '%s: sample rate not found\n' "$(basename "$audio_file")"
  fi
done
