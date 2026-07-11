#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=tools/local_env.sh
source "${SCRIPT_DIR}/local_env.sh"

ROOT_DIR="$(resolve_root_dir)"
init_stack_facts "${ROOT_DIR}"

glm_key="${XIAOZHI_GLM_API_KEY:-${GLM_API_KEY:-}}"
glm_key="${glm_key#GLM:}"

if [[ -z "${glm_key}" || "${glm_key}" == "YOUR_GLM_API_KEY" ]]; then
  echo "Set XIAOZHI_GLM_API_KEY or GLM_API_KEY before running this script." >&2
  exit 1
fi

if [[ "${glm_key}" == *"你的"* || "${glm_key}" == *"YOUR_"* || "${glm_key}" == "..." ]]; then
  echo "GLM key still looks like a placeholder. Export the real key before running this script." >&2
  exit 1
fi

if [[ ! "${glm_key}" =~ ^[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{8,}$ ]]; then
  echo "GLM key must look like a BigModel API key, for example id.secret." >&2
  exit 1
fi

if [[ ! -f "${XIAOZHI_LOCAL_ENV_FILE}" ]]; then
  if [[ ! -f "${ROOT_DIR}/.local.env.example" ]]; then
    echo "Missing ${ROOT_DIR}/.local.env.example" >&2
    exit 1
  fi
  cp "${ROOT_DIR}/.local.env.example" "${XIAOZHI_LOCAL_ENV_FILE}"
  echo "Created ${XIAOZHI_LOCAL_ENV_FILE}"
fi

update_env_value() {
  local name="$1"
  local value="$2"
  local file="$3"

  if grep -Eq "^${name}=" "${file}"; then
    sed -i.bak -e "s|^${name}=.*|${name}=${value}|" "${file}"
    rm -f "${file}.bak"
  else
    printf '%s=%s\n' "${name}" "${value}" >> "${file}"
  fi
}

update_env_value "XIAOZHI_GLM_API_KEY" "${glm_key}" "${XIAOZHI_LOCAL_ENV_FILE}"
echo "Updated XIAOZHI_GLM_API_KEY in ${XIAOZHI_LOCAL_ENV_FILE} (value hidden)"

"${SCRIPT_DIR}/render_local_config.sh"
"${SCRIPT_DIR}/doctor.sh"
