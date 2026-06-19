#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR=$(cd "$(dirname "$0")" && pwd)
IMAGE_NAME=${IMAGE_NAME:-qwen3-tts-rocm:local}
RENDER_NODE=${RENDER_NODE:-/dev/dri/renderD128}
HSA_OVERRIDE_GFX_VERSION=${HSA_OVERRIDE_GFX_VERSION:-11.0.2}
DOCKERFILE=${DOCKERFILE:-$PROJECT_DIR/Dockerfile.rocm}
SCRIPT_NAME=${SCRIPT_NAME:-vclone-gpu.py}

DOCKER=(docker)
if [[ "${USE_SUDO:-1}" == "1" ]]; then
  DOCKER=(sudo docker)
fi

mkdir -p "$PROJECT_DIR/.cache/huggingface"

"${DOCKER[@]}" build -t "$IMAGE_NAME" -f "$DOCKERFILE" "$PROJECT_DIR"

CONTAINER_CMD=(bash)
if [[ $# -gt 0 ]]; then
  CONTAINER_CMD=(python "$SCRIPT_NAME" "$@")
fi

"${DOCKER[@]}" run --rm -it \
  --cap-add=SYS_PTRACE \
  --security-opt seccomp=unconfined \
  --device=/dev/kfd \
  --device="$RENDER_NODE" \
  --group-add video \
  --ipc=host \
  --shm-size 8G \
  -e ROCR_VISIBLE_DEVICES=0 \
  -e HIP_VISIBLE_DEVICES=0 \
  -e CUDA_VISIBLE_DEVICES=0 \
  -e HSA_OVERRIDE_GFX_VERSION="$HSA_OVERRIDE_GFX_VERSION" \
  -v "$PROJECT_DIR:/workspace" \
  -v "$PROJECT_DIR/.cache/huggingface:/root/.cache/huggingface" \
  -w /workspace \
  "$IMAGE_NAME" \
  "${CONTAINER_CMD[@]}"
