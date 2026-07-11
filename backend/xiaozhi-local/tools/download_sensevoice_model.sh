#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_DIR="${ROOT_DIR}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/SenseVoiceSmall"
MODEL_FILE="${MODEL_DIR}/model.pt"
MODEL_URL="${SENSEVOICE_MODEL_URL:-https://www.modelscope.cn/models/iic/SenseVoiceSmall/resolve/master/model.pt}"

mkdir -p "${MODEL_DIR}"

if [[ -f "${MODEL_FILE}" ]]; then
  echo "model.pt already exists: ${MODEL_FILE}"
  exit 0
fi

echo "Downloading SenseVoiceSmall model.pt..."
echo "URL: ${MODEL_URL}"
curl -L --fail --continue-at - --output "${MODEL_FILE}" "${MODEL_URL}"
echo "Saved: ${MODEL_FILE}"
