# Grind Buddy Development Status

Updated: 2026-06-19

## Current Baseline

This repository has been consolidated into the single active Grind Buddy AI
hardware repo.

Active slices:

- `firmware/k230`: current K230 CanMV binary UART vision coprocessor.
- `firmware/esp32s3/xiaozhi-esp32`: current ESP32-S3 Xiaozhi firmware source.
- `backend/xiaozhi-local`: local Xiaozhi-compatible backend wrapper and server
  source.

Removed from the active tree:

- old UART JSON Lines K230 implementation
- old flat ESP32 parser implementation
- old UART simulator
- historical reference project copies

## Verified Before Consolidation

K230 source repo:

- `/Users/wq/k230-ai-companion`
- commit: `eea297b Implement K230 vision UART pipeline`
- main script: `dist/main_vision_uart_single.py`
- transport: binary frames, `A5 5A`, CRC-16/CCITT-FALSE, `921600` baud

ESP32 source checkout:

- source now migrated into `firmware/esp32s3/xiaozhi-esp32`
- commits:
  - `b6d948b Add SZPI S3 Grind Buddy integration`
  - `13b6761 Stabilize K230 vision wake gating`
- flashed bench log confirmed:
  - `Project name: xiaozhi`
  - `Grind Buddy UART integration started: uart=1 rx=10 baud=921600`
  - K230 binary frames reached ESP32.
  - visual wake fired only after stable frontal gaze.
  - startup `face.pose` report flood was removed.

Local backend source:

- `/Users/wq/xiaozhi-local-ai-hardware-stack`
- local OTA port: `8003`
- local Xiaozhi WebSocket port: `8001`
- model template:
  - ASR: FunASR / SenseVoiceSmall
  - LLM: ChatGLMLLM / glm-4-flash
  - VLLM: ChatGLMVLLM / glm-4v-flash
  - TTS: EdgeTTS / zh-CN-XiaoxiaoNeural

## Current In-Repo Status

K230:

- synced `src/main_vision_uart.py`
- synced binary protocol and UART publisher modules
- synced visual observation and head pose modules
- moved the old CanMV face-pose debug script to `examples/vision`
- synced `tools/build_k230_single.py`
- synced `dist/main_vision_uart_single.py`
- synced Python tests

ESP32-S3:

- synced full Xiaozhi ESP-IDF source tree
- synced `main/grind_buddy`
- synced `main/boards/szpi-s3`
- synced host tests under `tests/cpp`
- kept board selection in tracked defaults and left local `sdkconfig` ignored,
  so OTA IP and other machine-specific settings are generated per bench
- applied Xiaozhi integration directly to `main/CMakeLists.txt`,
  `main/Kconfig.projbuild`, and `main/application.*`

Backend:

- synced lightweight local backend wrapper into `backend/xiaozhi-local`
- kept scripts, tests, docs, config template, and Xiaozhi server source
- added local Silero VAD model sync and doctor coverage so the server fails
  preflight before runtime if `silero_vad.jit` is missing
- excluded real secrets, generated config, model weights, build output, caches,
  and large media resources

## 2026-06-17 Bench Result

Tonight's bench completed the K230 -> ESP32-S3 -> local Xiaozhi backend chain.

Verified:

- K230 was connected as CanMV/Kendryte on `/dev/cu.usbmodem0010000001`.
- K230 was running the UART vision pipeline and emitted binary vision events.
- ESP32-S3 SZPI board flashed from
  `firmware/esp32s3/xiaozhi-esp32/build-szpi-s3-local`.
- Correct flash/debug port for the final retest was `/dev/cu.usbmodem112301`.
- ESP32-S3 chip probe:
  - chip: ESP32-S3 QFN56 rev v0.2
  - MAC: `10:51:db:80:e2:e8`
  - USB mode: USB-Serial/JTAG
- Flash write and hash verification completed for:
  - bootloader
  - `xiaozhi.bin`
  - partition table
  - OTA data
  - `generated_assets.bin`
- Local backend generated ignored runtime config from `.local.env`.
- SenseVoiceSmall `model.pt` present locally.
- Silero VAD `silero_vad.jit` synced locally from the conda package into the
  upstream server model path.
- `backend/xiaozhi-local/tools/verify.sh` passed.
- `backend/xiaozhi-local/tools/doctor.sh` passed with no warnings while the
  server was running.
- Local backend listened on:
  - WebSocket: `8001`
  - HTTP OTA: `8003`
- HTTP OTA endpoint returned the device WebSocket URL:
  `ws://192.168.1.26:8001/xiaozhi/v1/`
- ESP32 booted:
  - `Project name: xiaozhi`
  - board SKU `szpi-s3`
  - app version `2.1.0`
  - `Grind Buddy UART integration started: uart=1 rx=10 baud=921600`
- ESP32 received K230 vision events:
  - `presence.enter`
  - `presence.leave`
  - `gaze.enter`
  - `gaze.leave`
- K230 frontal gaze opened Xiaozhi listening through visual wake:
  - `VisionWake: invoking Xiaozhi wake word`
  - `State: idle -> connecting -> listening -> speaking`
- Side gaze / leaving frame produced silent stop behavior:
  - `StopVoiceSession`
  - `VisionWake: silently stopping Xiaozhi listening`
- ESP32 after Wi-Fi provisioning:
  - requested OTA as device `10:51:db:80:e2:e8`
  - connected WebSocket from `192.168.1.32`
  - sent `hello`
  - sent wake detect text `你好小智`
  - received wake reply audio text including `我在这里，等候您的指令。`
    and `来啦来啦，请告诉我吧。`
  - completed at least one spoken user turn: `你会做什么呀？`
  - reported MCP server info `szpi-s3` version `2.1.0`

Known non-blocking warnings:

- Weather provider still uses placeholder QWeather config and logs a failed
  weather lookup.
- Voiceprint URL is not configured; voiceprint is disabled.
- Optional MCP server settings file is not configured.

## 2026-06-19 Gaze Stability Bench

Phase 3 visual stability work added bidirectional gaze hysteresis in the ESP32
K230 vision adapter:

- `gaze.enter` requires 300 ms of stable frontal gaze before the event is
  emitted.
- `gaze.leave` requires 150 ms of stable non-frontal gaze before the event is
  emitted.
- `face-lost` and K230 timeout still clear presence/gaze immediately.

Verified:

- ESP32-S3 rebuilt and flashed from `build-szpi-s3-local` on
  `/dev/cu.usbmodem112301`.
- Flash write and hash verification completed for bootloader, `xiaozhi.bin`,
  partition table, OTA data, and `generated_assets.bin`.
- K230 was connected as CanMV/Kendryte on `/dev/cu.usbmodem0010000001`.
- ESP32 booted `Project name: xiaozhi`, board `szpi-s3`, version `2.1.0`.
- ESP32 UART task started with `uart=1 rx=10 baud=921600`.
- K230 events reached ESP32 after boot.
- Visual wake still opened Xiaozhi WebSocket sessions to
  `ws://192.168.1.26:8001/xiaozhi/v1/`.
- Bench logs no longer showed the previous tens-of-milliseconds
  `gaze.enter` / `gaze.leave` flicker loop.
- Observed enter/leave transitions were separated by stable intervals and still
  produced the expected wake and silent-stop behavior.

## 2026-06-19 Visual Wake Gate Decision

ADR 0001 keeps the current visual wake path direct:

- stable K230 face pose and ESP32 gaze hysteresis remain the local gate before
  Xiaozhi listening;
- local VAD gating is not reintroduced for the current product slice;
- dormant local voice detection hooks may remain as future extension points;
- false-start work should first use recorded K230 pose and ESP32 event logs to
  tune visual thresholds.

## Deferred Work

- Continue only with work not covered by the 2026-06-17 bench result.
- Do not repeat the K230 -> ESP32 -> backend chain bring-up unless hardware,
  wiring, backend config, or flashed firmware changed.
- If another project overwrites the ESP32 firmware, reflash
  `firmware/esp32s3/xiaozhi-esp32` from this repository and continue with
  stability work.
- Visual threshold tuning is parked until there is a new bench reason to
  revisit it. Do not change face or gaze thresholds blindly; first capture
  numeric K230 pose values and ESP32 event timing for accepted and rejected
  cases.
- The old business backend idea is not part of the current local Xiaozhi
  runtime baseline. Revisit it only if the product needs account, persistence,
  task, sync, or cloud business features.

## Operating Rule

Future Grind Buddy development should start in:

```text
/Users/wq/grind-buddy-ai-hardware
```

External repos are deployment targets or upstream references. If a change is
made externally to build or flash faster, mirror it back here before treating it
as project state.
