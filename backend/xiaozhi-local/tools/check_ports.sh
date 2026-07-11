#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/local_env.sh
source "${SCRIPT_DIR}/local_env.sh"

ROOT_DIR="$(resolve_root_dir)"
init_stack_facts "${ROOT_DIR}"
if [[ -f "${XIAOZHI_LOCAL_ENV_FILE}" ]]; then
  load_local_env "${ROOT_DIR}"
fi

for port in "${XIAOZHI_WS_PORT}" "${XIAOZHI_HTTP_PORT}"; do
  echo "== TCP :${port} =="
  if command -v lsof >/dev/null 2>&1; then
    lsof -nP -iTCP:"${port}" -sTCP:LISTEN || true
  else
    echo "lsof not found"
  fi
done
