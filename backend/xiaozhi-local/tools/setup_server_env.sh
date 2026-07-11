#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_DIR="${ROOT_DIR}/server/xiaozhi-esp32-server/main/xiaozhi-server"
ENV_NAME="${XIAOZHI_CONDA_ENV:-xiaozhi-esp32-server}"
REQ_FILE="${SERVER_DIR}/requirements.txt"

if [[ "$(uname -s)" == "Darwin" && "$(uname -m)" == "arm64" ]]; then
  REQ_FILE="${SERVER_DIR}/requirements-macos-arm64.txt"
fi

if ! command -v conda >/dev/null 2>&1; then
  echo "conda is required. Install Miniforge or make conda available in PATH." >&2
  exit 1
fi

if ! conda env list | awk '{print $1}' | grep -qx "${ENV_NAME}"; then
  conda create -n "${ENV_NAME}" python=3.10 -y
fi

conda install -n "${ENV_NAME}" -y libopus ffmpeg
conda run -n "${ENV_NAME}" python -m pip install -r "${REQ_FILE}"
echo "Server env ready: ${ENV_NAME}"
