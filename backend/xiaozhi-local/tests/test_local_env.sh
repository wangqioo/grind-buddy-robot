#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RENDER="${REPO_ROOT}/tools/render_local_config.sh"
LOCAL_ENV_LIB="${REPO_ROOT}/tools/local_env.sh"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_contains() {
  local file="$1"
  local expected="$2"
  if ! grep -Fq -- "$expected" "$file"; then
    echo "Expected to find: $expected" >&2
    echo "--- ${file} ---" >&2
    cat "$file" >&2
    fail "missing expected text"
  fi
}

assert_not_contains() {
  local file="$1"
  local unexpected="$2"
  if grep -Fq -- "$unexpected" "$file"; then
    echo "Did not expect to find: $unexpected" >&2
    echo "--- ${file} ---" >&2
    cat "$file" >&2
    fail "unexpected text found"
  fi
}

make_fixture() {
  local root="$1"
  mkdir -p "${root}/server/xiaozhi-esp32-server/data"
  cp "${REPO_ROOT}/server/xiaozhi-esp32-server/data/.config.example.yaml" \
    "${root}/server/xiaozhi-esp32-server/data/.config.example.yaml"
}

write_local_env() {
  local root="$1"
  cat > "${root}/.local.env" <<'ENV'
XIAOZHI_LAN_IP=10.0.0.42
XIAOZHI_DEVICE_MAC=22:33:44:55:66:77
XIAOZHI_GLM_API_KEY=testglmkey1234567890.secretpart123456
XIAOZHI_WS_PORT=18001
XIAOZHI_HTTP_PORT=18003
ENV
}

write_multi_device_local_env() {
  local root="$1"
  cat > "${root}/.local.env" <<'ENV'
XIAOZHI_LAN_IP=10.0.0.42
XIAOZHI_DEVICE_MAC=22:33:44:55:66:77
XIAOZHI_DEVICE_MACS=22:33:44:55:66:77,88:99:aa:bb:cc:dd
XIAOZHI_GLM_API_KEY=testglmkey1234567890.secretpart123456
XIAOZHI_WS_PORT=18001
XIAOZHI_HTTP_PORT=18003
ENV
}

test_missing_local_env_fails() {
  local tmp out
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  make_fixture "$tmp"

  if XIAOZHI_STACK_ROOT="$tmp" bash "$RENDER" >"$out" 2>&1; then
    cat "$out" >&2
    fail "render should fail without .local.env"
  fi

  assert_contains "$out" "Missing"
  assert_contains "$out" ".local.env"
}

test_init_stack_facts_exports_default_paths() {
  local tmp out
  tmp="$(mktemp -d)"
  out="${tmp}/facts.txt"

  XIAOZHI_STACK_ROOT="$tmp" bash -c '
    source "$1"
    root="$(resolve_root_dir)"
    init_stack_facts "$root"
    printf "%s\n" \
      "$XIAOZHI_ROOT_DIR" \
      "$XIAOZHI_SERVER_ROOT" \
      "$XIAOZHI_SERVER_DIR" \
      "$XIAOZHI_CONFIG_EXAMPLE_FILE" \
      "$XIAOZHI_CONFIG_FILE" \
      "$XIAOZHI_SENSEVOICE_MODEL_FILE" \
      "$XIAOZHI_LOCAL_ENV_FILE" \
      "$XIAOZHI_WS_PORT" \
      "$XIAOZHI_HTTP_PORT" \
      "$XIAOZHI_CONDA_ENV_NAME"
  ' _ "$LOCAL_ENV_LIB" > "$out"

  assert_contains "$out" "$tmp"
  assert_contains "$out" "$tmp/server/xiaozhi-esp32-server"
  assert_contains "$out" "$tmp/server/xiaozhi-esp32-server/main/xiaozhi-server"
  assert_contains "$out" "$tmp/server/xiaozhi-esp32-server/data/.config.example.yaml"
  assert_contains "$out" "$tmp/server/xiaozhi-esp32-server/data/.config.yaml"
  assert_contains "$out" "$tmp/server/xiaozhi-esp32-server/main/xiaozhi-server/models/SenseVoiceSmall/model.pt"
  assert_contains "$out" "$tmp/.local.env"
  assert_contains "$out" "8001"
  assert_contains "$out" "8003"
  assert_contains "$out" "xiaozhi-esp32-server"
}

test_render_config_from_local_env() {
  local tmp config
  tmp="$(mktemp -d)"
  config="${tmp}/server/xiaozhi-esp32-server/data/.config.yaml"
  make_fixture "$tmp"
  write_local_env "$tmp"

  XIAOZHI_STACK_ROOT="$tmp" bash "$RENDER" >/dev/null

  assert_contains "$config" "port: 18001"
  assert_contains "$config" "http_port: 18003"
  assert_contains "$config" "websocket: ws://10.0.0.42:18001/xiaozhi/v1/"
  assert_contains "$config" "vision_explain: http://10.0.0.42:18003/mcp/vision/explain"
  assert_contains "$config" "- \"22:33:44:55:66:77\""
  assert_contains "$config" "LLM: ChatGLMLLM"
  assert_contains "$config" "VLLM: ChatGLMVLLM"
  assert_contains "$config" "api_key: testglmkey1234567890.secretpart123456"
  assert_not_contains "$config" "YOUR_MAC_LAN_IP"
  assert_not_contains "$config" "AA:BB:CC:DD:EE:FF"
  assert_not_contains "$config" "YOUR_GLM_API_KEY"
}

test_render_config_from_multi_device_local_env() {
  local tmp config
  tmp="$(mktemp -d)"
  config="${tmp}/server/xiaozhi-esp32-server/data/.config.yaml"
  make_fixture "$tmp"
  write_multi_device_local_env "$tmp"

  XIAOZHI_STACK_ROOT="$tmp" bash "$RENDER" >/dev/null

  assert_contains "$config" "- \"22:33:44:55:66:77\""
  assert_contains "$config" "- \"88:99:aa:bb:cc:dd\""
  assert_not_contains "$config" "AA:BB:CC:DD:EE:FF"
}

test_render_firmware_defaults() {
  local tmp defaults
  tmp="$(mktemp -d)"
  defaults="${tmp}/sdkconfig.defaults.local"
  make_fixture "$tmp"
  write_local_env "$tmp"

  XIAOZHI_STACK_ROOT="$tmp" bash "$RENDER" --firmware-defaults "$defaults" >/dev/null

  assert_contains "$defaults" 'CONFIG_IDF_TARGET="esp32s3"'
  assert_contains "$defaults" "CONFIG_BOARD_TYPE_SZPI_S3=y"
  assert_contains "$defaults" 'CONFIG_OTA_URL="http://10.0.0.42:18003/xiaozhi/ota/"'
  assert_contains "$defaults" "CONFIG_USE_DEVICE_AEC=y"
}

test_render_firmware_defaults_lan_ip_overrides_local_env() {
  local tmp defaults
  tmp="$(mktemp -d)"
  defaults="${tmp}/defaults"
  make_fixture "$tmp"
  write_local_env "$tmp"

  LAN_IP=10.0.0.99 XIAOZHI_STACK_ROOT="$tmp" bash "$RENDER" --firmware-defaults "$defaults" >/dev/null

  assert_contains "$defaults" 'CONFIG_OTA_URL="http://10.0.0.99:18003/xiaozhi/ota/"'
}

test_render_firmware_defaults_accepts_lan_ip_without_local_env() {
  local tmp defaults
  tmp="$(mktemp -d)"
  defaults="${tmp}/defaults"
  make_fixture "$tmp"

  LAN_IP=10.0.0.99 XIAOZHI_STACK_ROOT="$tmp" bash "$RENDER" --firmware-defaults "$defaults" >/dev/null

  assert_contains "$defaults" 'CONFIG_OTA_URL="http://10.0.0.99:8003/xiaozhi/ota/"'
}

test_render_fails_when_placeholder_leaks() {
  local tmp out
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  make_fixture "$tmp"
  write_local_env "$tmp"
  cat >> "${tmp}/server/xiaozhi-esp32-server/data/.config.example.yaml" <<'YAML'
leaked_placeholder: YOUR_NEW_REQUIRED_VALUE
YAML

  if XIAOZHI_STACK_ROOT="$tmp" bash "$RENDER" >"$out" 2>&1; then
    cat "$out" >&2
    fail "render should fail when placeholders remain in generated config"
  fi

  assert_contains "$out" "Rendered config still contains placeholders"
}

test_missing_local_env_fails
test_init_stack_facts_exports_default_paths
test_render_config_from_local_env
test_render_config_from_multi_device_local_env
test_render_firmware_defaults
test_render_firmware_defaults_lan_ip_overrides_local_env
test_render_firmware_defaults_accepts_lan_ip_without_local_env
test_render_fails_when_placeholder_leaks
echo "local env tests passed"
