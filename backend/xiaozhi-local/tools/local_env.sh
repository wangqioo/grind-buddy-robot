#!/usr/bin/env bash

resolve_root_dir() {
  if [[ -n "${XIAOZHI_STACK_ROOT:-}" ]]; then
    printf '%s\n' "${XIAOZHI_STACK_ROOT}"
  else
    cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd
  fi
}

init_stack_facts() {
  local root_dir="$1"

  XIAOZHI_ROOT_DIR="${root_dir}"
  XIAOZHI_SERVER_ROOT="${XIAOZHI_ROOT_DIR}/server/xiaozhi-esp32-server"
  XIAOZHI_SERVER_DIR="${XIAOZHI_SERVER_ROOT}/main/xiaozhi-server"
  XIAOZHI_CONFIG_EXAMPLE_FILE="${XIAOZHI_SERVER_ROOT}/data/.config.example.yaml"
  XIAOZHI_CONFIG_FILE="${XIAOZHI_SERVER_ROOT}/data/.config.yaml"
  XIAOZHI_SENSEVOICE_MODEL_FILE="${XIAOZHI_SERVER_DIR}/models/SenseVoiceSmall/model.pt"
  XIAOZHI_SILERO_VAD_MODEL_FILE="${XIAOZHI_SERVER_DIR}/models/snakers4_silero-vad/src/silero_vad/data/silero_vad.jit"
  XIAOZHI_LOCAL_ENV_FILE="${XIAOZHI_LOCAL_ENV:-${XIAOZHI_ROOT_DIR}/.local.env}"
  XIAOZHI_WS_PORT="${XIAOZHI_WS_PORT:-8001}"
  XIAOZHI_HTTP_PORT="${XIAOZHI_HTTP_PORT:-8003}"
  XIAOZHI_CONDA_ENV_NAME="${XIAOZHI_CONDA_ENV:-xiaozhi-esp32-server}"
}

load_local_env() {
  local root_dir="$1"
  init_stack_facts "${root_dir}"

  if [[ ! -f "${XIAOZHI_LOCAL_ENV_FILE}" ]]; then
    echo "Missing ${XIAOZHI_LOCAL_ENV_FILE}" >&2
    echo "Create it from .local.env.example and fill LAN IP, device MAC, and API key." >&2
    return 1
  fi

  set -a
  # shellcheck disable=SC1090
  source "${XIAOZHI_LOCAL_ENV_FILE}"
  set +a

  init_stack_facts "${root_dir}"
  XIAOZHI_WS_PORT="${XIAOZHI_WS_PORT:-8001}"
  XIAOZHI_HTTP_PORT="${XIAOZHI_HTTP_PORT:-8003}"
}

require_local_env_value() {
  local name="$1"
  local value="${!name:-}"

  if [[ -z "${value}" ]]; then
    echo "Missing ${name} in .local.env" >&2
    return 1
  fi
}

require_local_env() {
  require_local_env_value XIAOZHI_LAN_IP
  if [[ -z "${XIAOZHI_DEVICE_MACS:-}" ]]; then
    require_local_env_value XIAOZHI_DEVICE_MAC
  fi
  require_local_env_value XIAOZHI_GLM_API_KEY
}
