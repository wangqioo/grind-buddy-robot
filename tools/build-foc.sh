#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT/firmware/esp32-foc"

clang++ -std=c++17 test/test_face_tracking_policy.cpp \
  -o /tmp/grind_buddy_foc_face_tracking_policy
/tmp/grind_buddy_foc_face_tracking_policy

pio run
