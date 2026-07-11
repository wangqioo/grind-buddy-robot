# Grind Buddy Project Closure

Updated: 2026-06-19

## Closure State

The current product slice is closed for now:

```text
K230 binary UART vision
  -> ESP32-S3 Xiaozhi visual wake firmware
  -> Mac local Xiaozhi-compatible backend
```

This means the repo contains the active K230 firmware, ESP32-S3 Xiaozhi
firmware integration, local backend wrapper, docs, and verification commands for
the working bench chain. The old reference implementations and abandoned JSON
UART path are no longer part of the active tree.

## What Is Done

- Repository consolidation is complete.
- K230 binary UART generation and tests are in `firmware/k230`.
- ESP32-S3 SZPI Xiaozhi visual wake integration is in
  `firmware/esp32s3/xiaozhi-esp32`.
- Local Xiaozhi-compatible backend wrapper is in `backend/xiaozhi-local`.
- 2026-06-17 bench verified the full K230 -> ESP32-S3 -> backend chain.
- 2026-06-19 bench verified gaze hysteresis fixes and silent-stop behavior.
- Visual wake thresholds are centralized in
  `main/grind_buddy/grind_buddy_config.h`.
- ADR 0001 keeps the current runtime path direct: visual wake starts Xiaozhi
  listening without an extra local VAD gate.
- K230 UART fixture tests and backend OTA/WebSocket smoke tests are present.

## Final Verification

Run from `/Users/wq/grind-buddy-ai-hardware`.

K230:

```bash
cd firmware/k230
python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 -m compileall src tools
```

Latest closure run on 2026-06-19:

- single script generated at `firmware/k230/dist/main_vision_uart_single.py`;
- 24 K230 Python tests passed;
- `compileall` passed for `src` and `tools`.

ESP32 host tests:

```bash
cd firmware/esp32s3/xiaozhi-esp32
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

Latest closure run on 2026-06-19:

- all three ESP32 host test binaries compiled;
- all three host test binaries exited with code 0.

Backend:

```bash
cd backend/xiaozhi-local
tools/verify.sh
```

Latest closure run on 2026-06-19:

- doctor tests passed;
- local env tests passed;
- prepare local runtime tests passed;
- check ports local env tests passed;
- shell syntax, Python compile, and secret hygiene checks passed;
- backend smoke tests skipped optional runtime cases when optional dependencies
  were unavailable in the current Python environment.

## Parked Work

Visual threshold tuning is parked. Do not tune K230 face or ESP32 gaze
thresholds without a new bench reason and numeric logs.

If threshold work resumes, capture:

- K230 face confidence and pose values;
- ESP32 `presence.*` and `gaze.*` event timing;
- accepted wake cases;
- rejected or false-start cases.

The old business backend idea is outside the current local Xiaozhi baseline.
Revisit only if the product needs account, persistence, task, sync, or cloud
business features.

## Restart Conditions

Do not repeat the full Phase 2 bring-up unless one of these changed:

- ESP32 firmware was overwritten by another project;
- K230 script changed or was replaced on the board;
- wiring, UART pins, baud rate, or board changed;
- local backend config, model paths, or ports changed;
- bench behavior regressed.

If ESP32 firmware was overwritten, rebuild and flash
`firmware/esp32s3/xiaozhi-esp32` from this repository before debugging runtime
behavior.

## Known Local Workspace Note

At closure time, the only intentionally uncommitted local change was:

```text
backend/xiaozhi-local/server/xiaozhi-esp32-server/main/xiaozhi-server/config/assets/wakeup_words/ed76d459636c2481aec828516c1b4f54.wav
```

Treat it as runtime media unless its source and purpose are confirmed. Do not
commit it just to clean the worktree.
