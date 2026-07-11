#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"

"$ROOT/tools/build-k230.sh"
"$ROOT/tools/build-foc.sh"

cd "$ROOT/firmware/esp32s3-main"

clang++ -std=c++17 -I main \
  tests/cpp/test_k230_binary_protocol.cpp \
  main/grind_buddy/k230_binary_protocol.cc \
  -o /tmp/grind_buddy_test_k230_binary_protocol
/tmp/grind_buddy_test_k230_binary_protocol

clang++ -std=c++17 -I main \
  tests/cpp/test_k230_vision_adapter.cpp \
  main/grind_buddy/k230_vision_adapter.cc \
  -o /tmp/grind_buddy_test_k230_vision_adapter
/tmp/grind_buddy_test_k230_vision_adapter

clang++ -std=c++17 -I main \
  tests/cpp/test_grind_buddy_controller.cpp \
  main/grind_buddy/grind_buddy_controller.cc \
  main/grind_buddy/interaction_state.cc \
  -o /tmp/grind_buddy_test_controller
/tmp/grind_buddy_test_controller

clang++ -std=c++17 -I main \
  tests/cpp/test_grind_buddy_emotion_policy.cpp \
  main/grind_buddy/emotion_policy.cc \
  main/grind_buddy/interaction_state.cc \
  -o /tmp/grind_buddy_test_emotion_policy
/tmp/grind_buddy_test_emotion_policy
