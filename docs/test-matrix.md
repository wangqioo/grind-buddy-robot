# Grind Buddy Test Matrix

Updated: 2026-07-06

## Local Source Verification

| Area | Command | Expected |
|---|---|---|
| K230 single script | `cd firmware/k230 && python3 tools/build_k230_single.py` | `dist/main_vision_uart_single.py` generated |
| K230 tests | `cd firmware/k230 && python3 -m unittest discover -s tests -p 'test_*.py' -v` | 24 tests pass |
| K230 compile | `cd firmware/k230 && python3 -m compileall src tools` | no syntax errors |
| K230 deploy doc | `sed -n '1,220p' firmware/k230/README.md` | build, deploy, UART contract, and hardware verification steps are documented |
| ESP32 local defaults | `cd backend/xiaozhi-local && LAN_IP=192.168.1.26 tools/render_local_config.sh --firmware-defaults ../../firmware/esp32s3/xiaozhi-esp32/sdkconfig.defaults.local` | ignored local defaults file generated with SZPI board and OTA URL |
| ESP32 controller | `cd firmware/esp32s3/xiaozhi-esp32 && c++ ... test_grind_buddy_controller.cpp ...` | exit 0 |
| ESP32 binary protocol | `cd firmware/esp32s3/xiaozhi-esp32 && c++ ... test_k230_binary_protocol.cpp ...` | exit 0 |
| ESP32 vision adapter | `cd firmware/esp32s3/xiaozhi-esp32 && c++ ... test_k230_vision_adapter.cpp ...` | exit 0 |
| Backend verify | `cd backend/xiaozhi-local && tools/verify.sh` | no tracked secret and configured source checks pass |
| Backend preflight | `cd backend/xiaozhi-local && tools/doctor.sh` | config, SenseVoice, Silero VAD, conda, ffmpeg, ports, and ESP-IDF checks pass |

Latest local source check on 2026-07-06:

- K230 single script generation passed.
- K230 Python unit tests passed: 24 tests.
- K230 `compileall src tools` passed.
- ESP32 C++ host tests were not run in the Windows workspace because `g++` was
  not available on `PATH`.
- Backend shell checks were not run in the Windows workspace; run them from the
  Mac bench environment, WSL, or another Unix-like shell with dependencies.

## Hardware Verification

| Area | Method | Expected |
|---|---|---|
| K230 boot | CanMV runs `main_vision_uart_single.py` | verified 2026-06-17: K230 connected as CanMV/Kendryte on `/dev/cu.usbmodem0010000001` |
| K230 UART | ESP32 monitor log | verified 2026-06-17: ESP32 received K230 `presence.*` and `gaze.*` events at `921600` |
| K230 wiring | K230 `GPIO11` to ESP32-S3 `GPIO10`, common ground | verified 2026-06-17: ESP32 UART1 received vision frames |
| ESP32 flash | `idf.py -B build-szpi-s3-local -p /dev/cu.usbmodem112301 flash` | verified 2026-06-17: bootloader, app, partition table, OTA data, and generated assets written and hash verified |
| ESP32 boot | flash SZPI Xiaozhi build | verified 2026-06-17: `Project name: xiaozhi`, board `szpi-s3`, version `2.1.0` |
| ESP32 UART task | monitor log | verified 2026-06-17: `Grind Buddy UART integration started: uart=1 rx=10 baud=921600` |
| Visual wake | look directly at K230 camera | verified 2026-06-17: `VisionWake: invoking Xiaozhi wake word`, then `connecting -> listening -> speaking` |
| Visual stop | turn away or leave frame | verified 2026-06-17: `gaze.leave`, `StopVoiceSession`, and silent stop |
| Backend OTA | ESP32 OTA POST to Mac | verified 2026-06-17: device `10:51:db:80:e2:e8` received `ws://192.168.1.26:8001/xiaozhi/v1/` |
| Backend WebSocket | ESP32 connects after OTA | verified 2026-06-17: device connected from `192.168.1.32`, sent `hello`, and reported MCP `szpi-s3` `2.1.0` |
| Backend wake reply | ESP32 sends wake detect text | verified 2026-06-17: server received the Xiaozhi wake text and replied with audio |
| Backend voice | speak after visual wake | ASR/LLM/TTS loop completes |

## Known Local Runtime Requirements

- `.local.env` under `backend/xiaozhi-local`, ignored by Git.
- `data/.config.yaml` rendered from `data/.config.example.yaml`, ignored by
  Git.
- `models/SenseVoiceSmall/model.pt` downloaded locally, ignored by Git.
- `models/snakers4_silero-vad/src/silero_vad/data/silero_vad.jit` synced
  locally from the conda package by `tools/sync_silero_vad_model.sh`, ignored by
  Git.
- ESP-IDF available at `/Users/wq/esp-idf`.
- ESP32 `sdkconfig` and `sdkconfig.defaults.local` are generated locally and
  ignored by Git.
- Current SZPI serial port may differ; 2026-06-17 verified flash port was
  `/dev/cu.usbmodem112301`.
- If ESP32 has been flashed by another project, reflash from this repository
  before continuing product stability work.

Mac paths and serial ports in this matrix are bench examples. Use equivalent
local paths and ports when working from another machine.
