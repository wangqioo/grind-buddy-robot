#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-/dev/cu.wchusbserial1110}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

cd "$ROOT/firmware/esp32-foc"
pio run -t upload --upload-port "$PORT"
