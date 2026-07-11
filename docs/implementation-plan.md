# Grind Buddy Implementation Plan

Updated: 2026-06-19

## Goal

Use `/Users/wq/grind-buddy-ai-hardware` as the only active development
repository for the desktop companion hardware project.

The current product slice is:

```text
K230 binary UART vision
  -> ESP32-S3 Xiaozhi visual wake firmware
  -> Mac local Xiaozhi-compatible backend
```

## Phase 1: Repository Consolidation

- [x] Import current K230 binary UART source into `firmware/k230`.
- [x] Remove old K230 JSON Lines source, tests, and generated script.
- [x] Import current ESP32-S3 Xiaozhi firmware source into
  `firmware/esp32s3/xiaozhi-esp32`.
- [x] Remove old flat ESP32 JSON Lines parser source and tests.
- [x] Import local backend wrapper into `backend/xiaozhi-local`.
- [x] Exclude secrets, generated config, model weights, caches, and build
  output.
- [x] Remove old reference project copies from the active tree.
- [x] Run consolidated source verification.
- [x] Commit and push the consolidated repository.

## Phase 2: Hardware Reverification

- [x] Build K230 single script from this repo.
- [x] Copy or run `firmware/k230/dist/main_vision_uart_single.py` on the K230
  board.
- [x] Confirm K230 emits binary UART frames at `921600` baud.
- [x] Build SZPI ESP32-S3 firmware from this repo.
- [x] Flash SZPI ESP32-S3 firmware from this repo.
- [x] Confirm boot log shows `Grind Buddy UART integration started`.
- [x] Confirm frontal gaze opens Xiaozhi wake flow.
- [x] Confirm side/no-face silently stops listening.

Bench evidence from 2026-06-17:

- K230 was connected as CanMV/Kendryte on `/dev/cu.usbmodem0010000001`.
- ESP32-S3 was reflashed on `/dev/cu.usbmodem112301`; all flashed partitions
  passed hash verification.
- ESP32 booted `Project name: xiaozhi`, board `szpi-s3`, version `2.1.0`.
- ESP32 UART task started with `uart=1 rx=10 baud=921600`.
- ESP32 received K230 `presence.enter`, `gaze.enter`, and `gaze.leave` events.
- Visual wake opened Xiaozhi WebSocket session
  `ws://192.168.1.26:8001/xiaozhi/v1/`.
- Visual wake reached Xiaozhi listening/speaking and received backend replies.
- Looking away produced `gaze.leave`, `StopVoiceSession`, and silent stop
  behavior.

## Phase 3: Product Hardening

- [x] Reduce `gaze.enter` / `gaze.leave` flicker seen in the 2026-06-17 bench
  run. ESP32 adapter now confirms `gaze.enter` after 300 ms of stable frontal
  gaze and confirms `gaze.leave` after 150 ms of stable non-frontal gaze.
  Host tests and 2026-06-19 flash-and-bench verification passed.
- [x] Park K230 face and gaze threshold tuning until there is a new bench
  reason to revisit it. Do not tune thresholds blindly; if this resumes, first
  capture numeric K230 pose values and ESP32 event timing for accepted and
  rejected cases.
- [x] Make visual wake thresholds configurable from one ESP32 header. ESP32
  visual wake thresholds now live in `main/grind_buddy/grind_buddy_config.h`.
- [x] Add a recorded UART fixture test for the K230-to-ESP32 binary protocol.
  `tests/fixtures/k230_uart_stream.hex` covers noisy bytes, valid heartbeat,
  valid face, corrupt CRC recovery, and face-lost parsing.
- [x] Add a short backend smoke test that validates OTA and WebSocket routes
  without requiring model download. `tests/test_backend_smoke.py` exercises OTA
  WebSocket provisioning and WebSocket HTTP probe behavior; it skips when
  optional backend runtime dependencies are not installed.
- [x] Decide whether local VAD gating should be reintroduced after visual wake.
  ADR 0001 keeps visual wake direct for the current product slice and leaves
  local VAD hooks dormant unless real bench logs show repeated false starts
  after stable visual permission.

Do not repeat Phase 2 unless hardware or firmware was replaced. If ESP32 was
flashed by another project, reflash it from this repository first, then resume
from the Phase 3 flicker/stability work.

With threshold tuning parked, the current K230 -> ESP32-S3 -> local Xiaozhi
product slice is closed for now.

## Verification Commands

K230:

```bash
cd /Users/wq/grind-buddy-ai-hardware/firmware/k230
python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 -m compileall src tools
```

ESP32 host tests:

```bash
cd /Users/wq/grind-buddy-robot/firmware/esp32s3-main
c++ -std=c++20 -Wall -Wextra -Imain/grind_buddy \
  tests/cpp/test_grind_buddy_controller.cpp \
  main/grind_buddy/grind_buddy_controller.cc \
  main/grind_buddy/interaction_state.cc \
  -o /tmp/test_grind_buddy_controller
/tmp/test_grind_buddy_controller

c++ -std=c++20 -Wall -Wextra -Imain/grind_buddy \
  tests/cpp/test_k230_binary_protocol.cpp \
  main/grind_buddy/k230_binary_protocol.cc \
  -o /tmp/test_k230_binary_protocol
/tmp/test_k230_binary_protocol

c++ -std=c++20 -Wall -Wextra -Imain/grind_buddy \
  tests/cpp/test_k230_vision_adapter.cpp \
  main/grind_buddy/k230_vision_adapter.cc \
  -o /tmp/test_k230_vision_adapter
/tmp/test_k230_vision_adapter
```

Backend:

```bash
cd /Users/wq/grind-buddy-ai-hardware/backend/xiaozhi-local
tools/verify.sh
```
