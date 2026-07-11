#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

echo "verify: tests"
bash tests/test_doctor.sh
bash tests/test_local_env.sh
python3 tests/test_device_admission.py
python3 tests/test_connection_logging.py
python3 tests/test_ota_contract.py
python3 tests/test_runtime_profile.py
python3 tests/test_backend_smoke.py
bash tests/test_prepare_local_runtime.sh
bash tests/test_check_ports_local_env.sh

echo "verify: shell syntax"
bash -n \
  tools/check_ports.sh \
  tools/download_sensevoice_model.sh \
  tools/doctor.sh \
  tools/local_env.sh \
  tools/prepare_local_runtime.sh \
  tools/render_local_config.sh \
  tools/run_server.sh \
  tools/setup_server_env.sh \
  tools/sync_silero_vad_model.sh \
  tools/verify.sh \
  tests/test_check_ports_local_env.sh \
  tests/test_doctor.sh \
  tests/test_local_env.sh \
  tests/test_prepare_local_runtime.sh \
  tests/test_verify.sh

echo "verify: python compile"
python3 -m py_compile \
  server/xiaozhi-esp32-server/main/xiaozhi-server/app.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/config/config_loader.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/config/runtime_profile.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/device_admission.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/ota_contract.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/utils/connection_log.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/http_server.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/api/device_admin.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/api/ota_handler.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/providers/tools/device_mcp/mcp_handler.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/utils/audioRateController.py \
  server/xiaozhi-esp32-server/main/xiaozhi-server/core/websocket_server.py
python3 -m py_compile tests/test_backend_smoke.py

echo "verify: secret hygiene"
tracked_sensitive="$(git ls-files | grep -E '(^|/)\.config\.yaml$|(^|/)\.local\.env$|model\.pt$|(^|/)node_modules/' || true)"
if [[ -n "${tracked_sensitive}" ]]; then
  echo "tracked sensitive files:" >&2
  echo "${tracked_sensitive}" >&2
  exit 1
fi

tracked_secrets="$(git grep -nE \
  -e 'sk-[A-Za-z0-9_-]{20,}' \
  -e '(api_key|access_token|secret_key):[[:space:]]*["'\'']?sk-[A-Za-z0-9_-]{20,}' \
  -e 'DASHSCOPE_API_KEY[=:][[:space:]]*["'\'']?sk-[A-Za-z0-9_-]{20,}' \
  -e 'GLM_API_KEY[=:][[:space:]]*["'\'']?[A-Za-z0-9_-]{16,}\.[A-Za-z0-9_-]{8,}' \
  -- \
  ':!server/xiaozhi-esp32-server/data/.config.example.yaml' \
  ':!.local.env.example' \
  ':!server/xiaozhi-esp32-server/main/xiaozhi-server/config.yaml' \
  ':!tests/*' \
  ':!docs/security-and-secrets.md' || true)"
if [[ -n "${tracked_secrets}" ]]; then
  echo "tracked secret-like values:" >&2
  echo "${tracked_secrets}" >&2
  exit 1
fi

echo "verify: ok"
