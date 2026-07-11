#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/local_env.sh
source "${SCRIPT_DIR}/local_env.sh"

ROOT_DIR="$(resolve_root_dir)"
init_stack_facts "${ROOT_DIR}"

if [[ ! -f "${XIAOZHI_CONFIG_FILE}" ]]; then
  echo "Missing ${XIAOZHI_CONFIG_FILE}"
  echo "Create it from ${XIAOZHI_CONFIG_EXAMPLE_FILE} and fill in LAN IP, device MAC and API keys."
  exit 1
fi

if [[ ! -e "${XIAOZHI_SERVER_DIR}/data" ]]; then
  ln -s ../../data "${XIAOZHI_SERVER_DIR}/data"
fi

cd "${XIAOZHI_SERVER_DIR}"
exec conda run --no-capture-output -n "${XIAOZHI_CONDA_ENV_NAME}" python app.py
