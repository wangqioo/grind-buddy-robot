#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DOCTOR="${REPO_ROOT}/tools/doctor.sh"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_contains() {
  local file="$1"
  local expected="$2"
  if ! grep -Fq "$expected" "$file"; then
    echo "Expected to find: $expected" >&2
    echo "--- output ---" >&2
    cat "$file" >&2
    fail "missing expected text"
  fi
}

make_fixture() {
  local root="$1"
  mkdir -p "${root}/server/xiaozhi-esp32-server/data"
  mkdir -p "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/SenseVoiceSmall"
  mkdir -p "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/snakers4_silero-vad/src/silero_vad/data"
}

write_models() {
  local root="$1"
  printf 'model' > "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/SenseVoiceSmall/model.pt"
  printf 'model' > "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/snakers4_silero-vad/src/silero_vad/data/silero_vad.jit"
}

write_local_env() {
  local root="$1"
  local ws_port="${2:-8001}"
  local http_port="${3:-8003}"
  cat > "${root}/.local.env" <<'ENV'
XIAOZHI_LAN_IP=192.168.1.26
XIAOZHI_DEVICE_MAC=10:51:db:80:e2:e8
XIAOZHI_GLM_API_KEY=glmdoctorfixture1234567890.secretpart123456
ENV
  {
    echo "XIAOZHI_WS_PORT=${ws_port}"
    echo "XIAOZHI_HTTP_PORT=${http_port}"
  } >> "${root}/.local.env"
}

make_fake_bin() {
  local bin_dir="$1"
  mkdir -p "$bin_dir"
  cat > "${bin_dir}/conda" <<'FAKE_CONDA'
#!/usr/bin/env bash
if [[ "${1:-}" == "env" && "${2:-}" == "list" ]]; then
  echo "xiaozhi-esp32-server  /tmp/xiaozhi-esp32-server"
  exit 0
fi
echo "fake conda" >&2
exit 0
FAKE_CONDA
  cat > "${bin_dir}/ffmpeg" <<'FAKE_FFMPEG'
#!/usr/bin/env bash
echo "ffmpeg version fake"
FAKE_FFMPEG
  cat > "${bin_dir}/lsof" <<'FAKE_LSOF'
#!/usr/bin/env bash
exit 1
FAKE_LSOF
  chmod +x "${bin_dir}/conda" "${bin_dir}/ffmpeg" "${bin_dir}/lsof"
}

test_missing_config_fails() {
  local tmp out
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  make_fixture "$tmp"

  if XIAOZHI_STACK_ROOT="$tmp" bash "$DOCTOR" >"$out" 2>&1; then
    cat "$out" >&2
    fail "doctor should fail without data/.config.yaml"
  fi

  assert_contains "$out" "FAIL config file"
}

test_placeholder_config_fails() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"
  write_local_env "$tmp"
  write_models "$tmp"
  cat > "${tmp}/server/xiaozhi-esp32-server/data/.config.yaml" <<'YAML'
server:
  websocket: ws://YOUR_MAC_LAN_IP:8001/xiaozhi/v1/
  vision_explain: http://YOUR_MAC_LAN_IP:8003/mcp/vision/explain
YAML

  if PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" bash "$DOCTOR" >"$out" 2>&1; then
    cat "$out" >&2
    fail "doctor should fail when config still has placeholders"
  fi

  assert_contains "$out" "FAIL config placeholders"
}

test_ready_fixture_passes_with_warnings_allowed() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"
  write_local_env "$tmp"
  write_models "$tmp"
  cat > "${tmp}/server/xiaozhi-esp32-server/data/.config.yaml" <<'YAML'
server:
  websocket: ws://192.168.1.26:8001/xiaozhi/v1/
  vision_explain: http://192.168.1.26:8003/mcp/vision/explain
  auth:
    enabled: true
    allowed_devices:
      - "10:51:db:80:e2:e8"
LLM:
  ChatGLMLLM:
    api_key: glmdoctorfixture1234567890.secretpart123456
YAML

  PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" bash "$DOCTOR" >"$out" 2>&1

  assert_contains "$out" "PASS config file"
  assert_contains "$out" "PASS local env"
  assert_contains "$out" "PASS SenseVoice model"
  assert_contains "$out" "PASS Silero VAD model"
  assert_contains "$out" "PASS conda env"
  assert_contains "$out" "Doctor summary: ready"
}

test_doctor_uses_local_env_ports() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"
  write_local_env "$tmp" 18001 18003
  write_models "$tmp"
  cat > "${tmp}/server/xiaozhi-esp32-server/data/.config.yaml" <<'YAML'
server:
  websocket: ws://192.168.1.26:18001/xiaozhi/v1/
  vision_explain: http://192.168.1.26:18003/mcp/vision/explain
  auth:
    enabled: true
    allowed_devices:
      - "10:51:db:80:e2:e8"
LLM:
  ChatGLMLLM:
    api_key: glmdoctorfixture1234567890.secretpart123456
YAML

  PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" bash "$DOCTOR" >"$out" 2>&1

  assert_contains "$out" "WARN port 18001"
  assert_contains "$out" "WARN port 18003"
}

test_missing_config_fails
test_placeholder_config_fails
test_ready_fixture_passes_with_warnings_allowed
test_doctor_uses_local_env_ports
echo "doctor tests passed"
