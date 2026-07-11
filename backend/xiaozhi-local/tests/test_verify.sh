#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERIFY="${REPO_ROOT}/tools/verify.sh"

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

make_fake_path() {
  local bin_dir="$1"
  local log_file="$2"
  local git_output="${3:-}"
  local git_grep_output="${4:-}"
  mkdir -p "$bin_dir"

  cat > "${bin_dir}/bash" <<'FAKE_BASH'
#!/bin/bash
printf 'bash %s\n' "$*" >> "${VERIFY_FAKE_LOG}"
exit 0
FAKE_BASH

  cat > "${bin_dir}/python3" <<'FAKE_PYTHON'
#!/bin/bash
printf 'python3 %s\n' "$*" >> "${VERIFY_FAKE_LOG}"
exit 0
FAKE_PYTHON

  cat > "${bin_dir}/git" <<'FAKE_GIT'
#!/bin/bash
printf 'git %s\n' "$*" >> "${VERIFY_FAKE_LOG}"
if [[ "${1:-}" == "ls-files" && -n "${VERIFY_FAKE_GIT_OUTPUT:-}" ]]; then
  printf '%s\n' "${VERIFY_FAKE_GIT_OUTPUT}"
fi
if [[ "${1:-}" == "grep" && -n "${VERIFY_FAKE_GIT_GREP_OUTPUT:-}" ]]; then
  printf '%s\n' "${VERIFY_FAKE_GIT_GREP_OUTPUT}"
  exit 0
fi
if [[ "${1:-}" == "grep" ]]; then
  exit 1
fi
exit 0
FAKE_GIT

  chmod +x "${bin_dir}/bash" "${bin_dir}/python3" "${bin_dir}/git"
  export VERIFY_FAKE_LOG="$log_file"
  export VERIFY_FAKE_GIT_OUTPUT="$git_output"
  export VERIFY_FAKE_GIT_GREP_OUTPUT="$git_grep_output"
}

test_verify_runs_all_checks() {
  local tmp fake_bin log_file out
  tmp="$(mktemp -d)"
  fake_bin="${tmp}/bin"
  log_file="${tmp}/commands.log"
  out="${tmp}/out.txt"
  make_fake_path "$fake_bin" "$log_file"

  PATH="${fake_bin}:$PATH" /bin/bash "$VERIFY" >"$out"

  assert_contains "$out" "verify: tests"
  assert_contains "$out" "verify: shell syntax"
  assert_contains "$out" "verify: python compile"
  assert_contains "$out" "verify: secret hygiene"
  assert_contains "$out" "verify: ok"
  assert_contains "$log_file" "bash tests/test_doctor.sh"
  assert_contains "$log_file" "bash tests/test_local_env.sh"
  assert_contains "$log_file" "python3 tests/test_device_admission.py"
  assert_contains "$log_file" "python3 tests/test_ota_contract.py"
  assert_contains "$log_file" "python3 tests/test_runtime_profile.py"
  assert_contains "$log_file" "bash tests/test_prepare_local_runtime.sh"
  assert_contains "$log_file" "bash tests/test_check_ports_local_env.sh"
  assert_contains "$log_file" "bash -n"
  assert_contains "$log_file" "tools/prepare_local_runtime.sh"
  assert_contains "$log_file" "python3 -m py_compile"
  assert_contains "$log_file" "config/runtime_profile.py"
  assert_contains "$log_file" "core/device_admission.py"
  assert_contains "$log_file" "core/ota_contract.py"
  assert_contains "$log_file" "git ls-files"
}

test_verify_fails_when_sensitive_files_are_tracked() {
  local tmp fake_bin log_file out
  tmp="$(mktemp -d)"
  fake_bin="${tmp}/bin"
  log_file="${tmp}/commands.log"
  out="${tmp}/out.txt"
  make_fake_path "$fake_bin" "$log_file" "server/xiaozhi-esp32-server/data/.config.yaml"

  if PATH="${fake_bin}:$PATH" /bin/bash "$VERIFY" >"$out" 2>&1; then
    cat "$out" >&2
    fail "verify should fail when tracked sensitive files exist"
  fi

  assert_contains "$out" "tracked sensitive files"
  assert_contains "$out" "server/xiaozhi-esp32-server/data/.config.yaml"
}

test_verify_fails_when_tracked_secret_is_found() {
  local tmp fake_bin log_file out
  tmp="$(mktemp -d)"
  fake_bin="${tmp}/bin"
  log_file="${tmp}/commands.log"
  out="${tmp}/out.txt"
  make_fake_path "$fake_bin" "$log_file" "" "docs/example.md:api_key: sk-1234567890abcdef1234567890abcdef"

  if PATH="${fake_bin}:$PATH" /bin/bash "$VERIFY" >"$out" 2>&1; then
    cat "$out" >&2
    fail "verify should fail when tracked secrets are found"
  fi

  assert_contains "$out" "tracked secret-like values"
  assert_contains "$out" "docs/example.md"
}

test_verify_fails_when_tracked_glm_secret_is_found() {
  local tmp fake_bin log_file out
  tmp="$(mktemp -d)"
  fake_bin="${tmp}/bin"
  log_file="${tmp}/commands.log"
  out="${tmp}/out.txt"
  make_fake_path "$fake_bin" "$log_file" "" "docs/example.md:GLM_API_KEY=glmleak1234567890.secretpart123456"

  if PATH="${fake_bin}:$PATH" /bin/bash "$VERIFY" >"$out" 2>&1; then
    cat "$out" >&2
    fail "verify should fail when tracked GLM secrets are found"
  fi

  assert_contains "$out" "tracked secret-like values"
  assert_contains "$out" "docs/example.md"
}

test_verify_runs_all_checks
test_verify_fails_when_sensitive_files_are_tracked
test_verify_fails_when_tracked_secret_is_found
test_verify_fails_when_tracked_glm_secret_is_found
echo "verify tests passed"
