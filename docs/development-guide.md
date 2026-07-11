# Grind Buddy Development Guide

Updated: 2026-07-06

This repository is the active development home for Grind Buddy AI hardware.
Treat older local checkouts as references only.

## Where To Make Changes

| Work area | Directory | Notes |
|---|---|---|
| K230 camera, face pose, UART frame generation | `firmware/k230/` | Regenerate `dist/main_vision_uart_single.py` after editing `src/`. |
| ESP32 visual wake, state machine, SZPI board integration | `firmware/esp32s3/xiaozhi-esp32/` | Grind Buddy code lives mostly under `main/grind_buddy/` and `main/boards/szpi-s3/`. |
| OTA, WebSocket voice backend, provider config, local scripts | `backend/xiaozhi-local/` | Keep secrets, generated config, model weights, logs, and caches out of Git. |
| Project decisions and verification notes | `docs/` | Update docs in the same change when behavior or setup changes. |

## Current Runtime Chain

```text
K230 CanMV vision coprocessor
  -> binary UART frames at 921600 baud
ESP32-S3 SZPI Xiaozhi firmware
  -> visual wake and silent listening stop
Local Xiaozhi-compatible backend
  -> OTA, WebSocket voice, ASR, LLM, VLLM, TTS
```

K230 is only the vision coprocessor. Do not put product audio, display, Wi-Fi,
cloud state, or voice-session ownership back into K230 code unless the
architecture decision is intentionally reversed.

## Before Feature Work

1. Read `docs/architecture.md` for system ownership.
2. Read `docs/development-status.md` for the last verified bench state.
3. Read `docs/adr/0001-visual-wake-without-local-vad-gating.md` before changing
   visual wake or local VAD behavior.
4. Read `docs/esp32-emote-gen-guide.md` before continuing ESP32 expression or
   emotion-display work.
5. Check `docs/test-matrix.md` for the relevant verification commands.

## Standard Checks

K230:

```bash
cd firmware/k230
python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 -m compileall src tools
```

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

Backend:

```bash
cd backend/xiaozhi-local
tools/verify.sh
tools/doctor.sh
```

The ESP32 and backend commands assume a Unix-like shell with ESP-IDF and the
backend runtime dependencies installed. On Windows, use WSL, Git Bash, or the
original Mac bench environment.

## Git Hygiene

Do not commit:

- `.local.env`
- generated server config such as `data/.config.yaml`
- model weights
- ESP-IDF `build*`, `managed_components`, `sdkconfig`
- logs, caches, local databases, or virtual environments

The generated K230 single script is intentionally tracked at
`firmware/k230/dist/main_vision_uart_single.py`.
