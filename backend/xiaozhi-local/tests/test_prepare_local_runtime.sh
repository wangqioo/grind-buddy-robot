#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREPARE="${REPO_ROOT}/tools/prepare_local_runtime.sh"

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
  mkdir -p "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/SenseVoiceSmall"
  mkdir -p "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/snakers4_silero-vad/src/silero_vad/data"
  cp "${REPO_ROOT}/server/xiaozhi-esp32-server/data/.config.example.yaml" \
    "${root}/server/xiaozhi-esp32-server/data/.config.example.yaml"
  cp "${REPO_ROOT}/.local.env.example" "${root}/.local.env.example"
  printf 'model' > "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/SenseVoiceSmall/model.pt"
  printf 'model' > "${root}/server/xiaozhi-esp32-server/main/xiaozhi-server/models/snakers4_silero-vad/src/silero_vad/data/silero_vad.jit"
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

test_prepare_requires_glm_key() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"

  if PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" bash "$PREPARE" >"$out" 2>&1; then
    cat "$out" >&2
    fail "prepare should fail without a GLM key in the environment"
  fi

  assert_contains "$out" "Set XIAOZHI_GLM_API_KEY or GLM_API_KEY"
}

test_prepare_rejects_placeholder_glm_key() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"

  if PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" GLM_API_KEY="你的真实key" \
    bash "$PREPARE" >"$out" 2>&1; then
    cat "$out" >&2
    fail "prepare should fail when GLM key is still a placeholder"
  fi

  assert_contains "$out" "GLM key still looks like a placeholder"
}

test_prepare_rejects_placeholder_key_with_glm_prefix() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"

  if PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" GLM_API_KEY="GLM:你的实际智谱Key" \
    bash "$PREPARE" >"$out" 2>&1; then
    cat "$out" >&2
    fail "prepare should fail when GLM-prefixed key is still a placeholder"
  fi

  assert_contains "$out" "GLM key still looks like a placeholder"
}

test_prepare_rejects_malformed_glm_key() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"

  if PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" GLM_API_KEY="not-a-real-key" \
    bash "$PREPARE" >"$out" 2>&1; then
    cat "$out" >&2
    fail "prepare should fail when GLM key format is invalid"
  fi

  assert_contains "$out" "GLM key must look like a BigModel API key"
}

test_prepare_creates_local_env_renders_config_and_does_not_print_key() {
  local tmp out fake_bin secret config
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  secret="glmtestpreparelocalruntime1234567890.secretpart123456"
  config="${tmp}/server/xiaozhi-esp32-server/data/.config.yaml"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"

  PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" GLM_API_KEY="GLM:${secret}" \
    bash "$PREPARE" >"$out" 2>&1

  assert_contains "${tmp}/.local.env" "XIAOZHI_GLM_API_KEY=${secret}"
  assert_not_contains "${tmp}/.local.env" "GLM:${secret}"
  assert_contains "$config" "api_key: ${secret}"
  assert_contains "$config" "LLM: ChatGLMLLM"
  assert_contains "$config" "VLLM: ChatGLMVLLM"
  assert_contains "$out" "Wrote ${config}"
  assert_contains "$out" "Doctor summary: ready"
  assert_not_contains "$out" "$secret"
}

test_prepare_updates_existing_local_env_from_xiaozhi_key() {
  local tmp out fake_bin secret
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  secret="existinglocalenv1234567890abcd.secretpart123456"
  make_fixture "$tmp"
  make_fake_bin "$fake_bin"
  cp "${tmp}/.local.env.example" "${tmp}/.local.env"

  PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" XIAOZHI_GLM_API_KEY="$secret" \
    bash "$PREPARE" >"$out" 2>&1

  assert_contains "${tmp}/.local.env" "XIAOZHI_GLM_API_KEY=${secret}"
  assert_not_contains "${tmp}/.local.env" "YOUR_GLM_API_KEY"
  assert_not_contains "$out" "$secret"
}

test_prepare_requires_glm_key
test_prepare_rejects_placeholder_glm_key
test_prepare_rejects_placeholder_key_with_glm_prefix
test_prepare_rejects_malformed_glm_key
test_prepare_creates_local_env_renders_config_and_does_not_print_key
test_prepare_updates_existing_local_env_from_xiaozhi_key
echo "prepare local runtime tests passed"
