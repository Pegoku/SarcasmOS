#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR=$(cd "$(dirname "$0")" && pwd)
IMAGE_NAME=${IMAGE_NAME:-qwen3-tts-rocm:local}

DOCKER=(docker)
if [[ "${USE_SUDO:-1}" == "1" ]]; then
  DOCKER=(sudo docker)
fi

mkdir -p "$PROJECT_DIR/.cache/huggingface"

"${DOCKER[@]}" build -t "$IMAGE_NAME" -f "$PROJECT_DIR/Dockerfile.rocm" "$PROJECT_DIR"

CONTAINER_CMD=(bash)
if [[ $# -gt 0 ]]; then
  CONTAINER_CMD=(python vclone.py "$@")
fi

"${DOCKER[@]}" run --rm -it \
  --cap-add=SYS_PTRACE \
  --security-opt seccomp=unconfined \
  --device=/dev/kfd \
  --device=/dev/dri \
  --group-add video \
  --ipc=host \
  --shm-size 8G \
  -v "$PROJECT_DIR:/workspace" \
  -v "$PROJECT_DIR/.cache/huggingface:/root/.cache/huggingface" \
  -w /workspace \
  "$IMAGE_NAME" \
  "${CONTAINER_CMD[@]}"
