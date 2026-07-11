#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/firmware/esp32s3-main"

. /Users/wq/esp-idf/export.sh >/tmp/grind-buddy-idf-export.log
idf.py -B build-szpi-s3-local build
