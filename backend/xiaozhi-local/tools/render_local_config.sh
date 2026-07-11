#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/local_env.sh
source "${SCRIPT_DIR}/local_env.sh"

ROOT_DIR="$(resolve_root_dir)"
init_stack_facts "${ROOT_DIR}"

usage() {
  echo "Usage: $0 [--firmware-defaults PATH]"
}

render_firmware_defaults() {
  local output_file="$1"
  cat > "${output_file}" <<EOF
CONFIG_IDF_TARGET="esp32s3"
CONFIG_BOARD_TYPE_SZPI_S3=y
CONFIG_OTA_URL="http://${XIAOZHI_LAN_IP}:${XIAOZHI_HTTP_PORT}/xiaozhi/ota/"
CONFIG_USE_DEVICE_AEC=y
EOF
}

render_server_config() {
  if [[ ! -f "${XIAOZHI_CONFIG_EXAMPLE_FILE}" ]]; then
    echo "Missing ${XIAOZHI_CONFIG_EXAMPLE_FILE}" >&2
    exit 1
  fi

  sed \
    -e "s|YOUR_MAC_LAN_IP|${XIAOZHI_LAN_IP}|g" \
    -e "s|port: 8001|port: ${XIAOZHI_WS_PORT}|g" \
    -e "s|http_port: 8003|http_port: ${XIAOZHI_HTTP_PORT}|g" \
    -e "s|:8001|:${XIAOZHI_WS_PORT}|g" \
    -e "s|:8003|:${XIAOZHI_HTTP_PORT}|g" \
    -e "s|AA:BB:CC:DD:EE:FF|${XIAOZHI_DEVICE_MAC}|g" \
    -e "s|YOUR_GLM_API_KEY|${XIAOZHI_GLM_API_KEY}|g" \
    "${XIAOZHI_CONFIG_EXAMPLE_FILE}" > "${XIAOZHI_CONFIG_FILE}"
  validate_server_config
  echo "Wrote ${XIAOZHI_CONFIG_FILE}"
}

validate_server_config() {
  if grep -Eq 'YOUR_|AA:BB:CC:DD:EE:FF|你的|your_token|YOUR_LINKERAI_ACCESS_TOKEN' "${XIAOZHI_CONFIG_FILE}"; then
    echo "Rendered config still contains placeholders: ${XIAOZHI_CONFIG_FILE}" >&2
    return 1
  fi

  if ! grep -Fq "websocket: ws://${XIAOZHI_LAN_IP}:${XIAOZHI_WS_PORT}/xiaozhi/v1/" "${XIAOZHI_CONFIG_FILE}"; then
    echo "Rendered config websocket does not match .local.env" >&2
    return 1
  fi

  if ! grep -Fq "vision_explain: http://${XIAOZHI_LAN_IP}:${XIAOZHI_HTTP_PORT}/mcp/vision/explain" "${XIAOZHI_CONFIG_FILE}"; then
    echo "Rendered config HTTP endpoint does not match .local.env" >&2
    return 1
  fi

  if ! grep -Fq "\"${XIAOZHI_DEVICE_MAC}\"" "${XIAOZHI_CONFIG_FILE}"; then
    echo "Rendered config allowed device does not match .local.env" >&2
    return 1
  fi

  if ! grep -Fq "api_key: ${XIAOZHI_GLM_API_KEY}" "${XIAOZHI_CONFIG_FILE}"; then
    echo "Rendered config API key does not match .local.env" >&2
    return 1
  fi
}

main() {
  local defaults_output=""

  while [[ $# -gt 0 ]]; do
    case "$1" in
      --firmware-defaults)
        if [[ $# -lt 2 ]]; then
          usage >&2
          exit 1
        fi
        defaults_output="$2"
        shift 2
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        usage >&2
        exit 1
        ;;
    esac
  done

  if [[ -f "${XIAOZHI_LOCAL_ENV_FILE}" ]]; then
    load_local_env "${ROOT_DIR}"
  elif [[ -z "${defaults_output}" ]]; then
    echo "Missing ${XIAOZHI_LOCAL_ENV_FILE}" >&2
    echo "Create it from .local.env.example and fill LAN IP, device MAC, and API key." >&2
    exit 1
  else
    XIAOZHI_LAN_IP="${XIAOZHI_LAN_IP:-${LAN_IP:-}}"
    XIAOZHI_WS_PORT="${XIAOZHI_WS_PORT:-8001}"
    XIAOZHI_HTTP_PORT="${XIAOZHI_HTTP_PORT:-8003}"
  fi

  if [[ -n "${defaults_output}" ]]; then
    XIAOZHI_LAN_IP="${LAN_IP:-${XIAOZHI_LAN_IP:-}}"
    require_local_env_value XIAOZHI_LAN_IP
    render_firmware_defaults "${defaults_output}"
    echo "Wrote ${defaults_output}"
  else
    require_local_env
    render_server_config
  fi
}

main "$@"
