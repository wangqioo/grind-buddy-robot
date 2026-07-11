#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/local_env.sh
source "${SCRIPT_DIR}/local_env.sh"

ROOT_DIR="$(resolve_root_dir)"
init_stack_facts "${ROOT_DIR}"
IDF_PATH="${IDF_PATH:-${HOME}/esp-idf}"

failures=0
warnings=0

print_check() {
  local status="$1"
  local name="$2"
  local detail="$3"
  printf '%-4s %s - %s\n' "$status" "$name" "$detail"
}

pass() {
  print_check "PASS" "$1" "$2"
}

warn() {
  warnings=$((warnings + 1))
  print_check "WARN" "$1" "$2"
}

fail() {
  failures=$((failures + 1))
  print_check "FAIL" "$1" "$2"
}

check_config() {
  if [[ ! -f "${XIAOZHI_CONFIG_FILE}" ]]; then
    fail "config file" "missing ${XIAOZHI_CONFIG_FILE}"
    return
  fi

  pass "config file" "${XIAOZHI_CONFIG_FILE}"

  if grep -Eq 'YOUR_|AA:BB:CC:DD:EE:FF|你的|your_token|YOUR_LINKERAI_ACCESS_TOKEN' "${XIAOZHI_CONFIG_FILE}"; then
    fail "config placeholders" "replace placeholders in data/.config.yaml"
  else
    pass "config placeholders" "no obvious placeholders found"
  fi

  if grep -Eq 'websocket:[[:space:]]*ws://(localhost|127\.0\.0\.1|0\.0\.0\.0)' "${XIAOZHI_CONFIG_FILE}"; then
    fail "websocket address" "use a LAN IP reachable by ESP32, not localhost"
  elif grep -Eq 'websocket:[[:space:]]*ws://' "${XIAOZHI_CONFIG_FILE}"; then
    pass "websocket address" "LAN-style websocket configured"
  else
    warn "websocket address" "server.websocket not found in data/.config.yaml"
  fi

  if grep -Eq 'allowed_devices:' "${XIAOZHI_CONFIG_FILE}"; then
    pass "device auth" "allowed_devices section present"
  else
    warn "device auth" "allowed_devices section not found"
  fi
}

check_local_env() {
  if [[ ! -f "${XIAOZHI_LOCAL_ENV_FILE}" ]]; then
    warn "local env" "missing ${XIAOZHI_LOCAL_ENV_FILE}; use .local.env.example for repeatable setup"
    return
  fi

  if load_local_env "${ROOT_DIR}" && require_local_env; then
    pass "local env" "${XIAOZHI_LOCAL_ENV_FILE}"
  else
    fail "local env" "missing required values in ${XIAOZHI_LOCAL_ENV_FILE}"
  fi
}

check_model() {
  if [[ -s "${XIAOZHI_SENSEVOICE_MODEL_FILE}" ]]; then
    pass "SenseVoice model" "${XIAOZHI_SENSEVOICE_MODEL_FILE}"
  else
    fail "SenseVoice model" "missing or empty ${XIAOZHI_SENSEVOICE_MODEL_FILE}"
  fi

  if [[ -s "${XIAOZHI_SILERO_VAD_MODEL_FILE}" ]]; then
    pass "Silero VAD model" "${XIAOZHI_SILERO_VAD_MODEL_FILE}"
  else
    fail "Silero VAD model" "missing or empty ${XIAOZHI_SILERO_VAD_MODEL_FILE}; run tools/sync_silero_vad_model.sh"
  fi
}

check_conda() {
  if ! command -v conda >/dev/null 2>&1; then
    fail "conda command" "conda is not available in PATH"
    fail "conda env" "cannot check ${ENV_NAME} without conda"
    return
  fi

  pass "conda command" "$(command -v conda)"

  if conda env list 2>/dev/null | awk '{print $1}' | grep -qx "${XIAOZHI_CONDA_ENV_NAME}"; then
    pass "conda env" "${XIAOZHI_CONDA_ENV_NAME}"
  else
    fail "conda env" "${XIAOZHI_CONDA_ENV_NAME} not found; run tools/setup_server_env.sh"
  fi
}

check_ffmpeg() {
  if command -v ffmpeg >/dev/null 2>&1; then
    pass "ffmpeg command" "$(command -v ffmpeg)"
  else
    fail "ffmpeg command" "ffmpeg is not available in PATH"
  fi
}

check_port() {
  local port="$1"
  if ! command -v lsof >/dev/null 2>&1; then
    warn "port ${port}" "lsof not available; cannot inspect listener"
    return
  fi

  if lsof -nP -iTCP:"${port}" -sTCP:LISTEN >/dev/null 2>&1; then
    pass "port ${port}" "listening"
  else
    warn "port ${port}" "not listening yet; start tools/run_server.sh when ready"
  fi
}

check_idf() {
  if [[ -f "${IDF_PATH}/export.sh" ]]; then
    pass "ESP-IDF" "${IDF_PATH}/export.sh"
  else
    warn "ESP-IDF" "missing ${IDF_PATH}/export.sh; flashing needs IDF_PATH"
  fi
}

echo "Xiaozhi local hardware stack doctor"
echo "Root: ${ROOT_DIR}"
echo

check_config
check_local_env
check_model
check_conda
check_ffmpeg
check_port "${XIAOZHI_WS_PORT}"
check_port "${XIAOZHI_HTTP_PORT}"
check_idf

echo
if [[ "${failures}" -gt 0 ]]; then
  echo "Doctor summary: not ready (${failures} failure(s), ${warnings} warning(s))"
  exit 1
fi

echo "Doctor summary: ready (${warnings} warning(s))"
