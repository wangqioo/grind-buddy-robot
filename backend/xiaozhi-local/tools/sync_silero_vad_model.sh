#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/local_env.sh
source "${SCRIPT_DIR}/local_env.sh"

ROOT_DIR="$(resolve_root_dir)"
init_stack_facts "${ROOT_DIR}"

if [[ -s "${XIAOZHI_SILERO_VAD_MODEL_FILE}" ]]; then
  echo "silero_vad.jit already exists: ${XIAOZHI_SILERO_VAD_MODEL_FILE}"
  exit 0
fi

if ! command -v conda >/dev/null 2>&1; then
  echo "conda is required to locate the installed silero_vad package." >&2
  exit 1
fi

source_model="$(
  conda run -n "${XIAOZHI_CONDA_ENV_NAME}" python -c \
    'import pathlib, silero_vad; model = pathlib.Path(silero_vad.__file__).parent / "data" / "silero_vad.jit"; print(model if model.is_file() and model.stat().st_size > 0 else "")'
)"

if [[ -z "${source_model}" || ! -s "${source_model}" ]]; then
  echo "Missing silero_vad.jit in conda env ${XIAOZHI_CONDA_ENV_NAME}; run tools/setup_server_env.sh first." >&2
  exit 1
fi

mkdir -p "$(dirname "${XIAOZHI_SILERO_VAD_MODEL_FILE}")"
cp "${source_model}" "${XIAOZHI_SILERO_VAD_MODEL_FILE}"
echo "Copied silero_vad.jit from ${source_model}"
echo "Saved: ${XIAOZHI_SILERO_VAD_MODEL_FILE}"
