#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CHECK_PORTS="${REPO_ROOT}/tools/check_ports.sh"

fail() {
  echo "FAIL: $*" >&2
  exit 1
}

assert_contains() {
  local file="$1"
  local expected="$2"
  if ! grep -Fq -- "$expected" "$file"; then
    echo "Expected to find: $expected" >&2
    echo "--- output ---" >&2
    cat "$file" >&2
    fail "missing expected text"
  fi
}

test_check_ports_uses_local_env_ports() {
  local tmp out fake_bin
  tmp="$(mktemp -d)"
  out="${tmp}/out.txt"
  fake_bin="${tmp}/bin"
  mkdir -p "$fake_bin"
  cat > "${tmp}/.local.env" <<'ENV'
XIAOZHI_LAN_IP=10.0.0.42
XIAOZHI_DEVICE_MAC=22:33:44:55:66:77
XIAOZHI_GLM_API_KEY=testglmkey1234567890.secretpart123456
XIAOZHI_WS_PORT=18001
XIAOZHI_HTTP_PORT=18003
ENV
  cat > "${fake_bin}/lsof" <<'FAKE_LSOF'
#!/usr/bin/env bash
echo "fake lsof $*"
FAKE_LSOF
  chmod +x "${fake_bin}/lsof"

  PATH="${fake_bin}:$PATH" XIAOZHI_STACK_ROOT="$tmp" bash "$CHECK_PORTS" >"$out"

  assert_contains "$out" "== TCP :18001 =="
  assert_contains "$out" "== TCP :18003 =="
  assert_contains "$out" "fake lsof -nP -iTCP:18001 -sTCP:LISTEN"
  assert_contains "$out" "fake lsof -nP -iTCP:18003 -sTCP:LISTEN"
}

test_check_ports_uses_local_env_ports
echo "check ports local env tests passed"
