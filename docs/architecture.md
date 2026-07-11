# Grind Buddy Architecture

Updated: 2026-07-06

## Canonical Repository

`/Users/wq/grind-buddy-ai-hardware` is now the canonical project repository.

The previous separate working checkouts are treated as sources or upstream
references only:

- `/Users/wq/k230-ai-companion`: source of the current K230 binary UART flow.
- `/Users/wq/Workshop/MCU/xiaozhi-project/xiaozhi-esp32`: historical Xiaozhi
  firmware source used for the current ESP32-S3 snapshot.
- `/Users/wq/xiaozhi-local-ai-hardware-stack`: historical source of the local
  backend wrapper now migrated under `backend/xiaozhi-local`.

New architecture, firmware, backend, and test work should be made in this
repository first.

## Product Definition

Grind Buddy is a gaze-responsive desktop companion robot.

The product is not primarily a chat assistant or a screen-notification device.
Its default companionship comes from motion, eye contact, facial expression,
small sounds, and low-friction presence. Speech remains available, but the
device should not need to talk in order to feel responsive.

The target hardware is a small three-axis gimbal robot with an expressive face,
camera, microphone, speaker, and motion-control capability.

The core interaction is gaze-triggered response:

- When the user looks at the device, it may enter an active response mode.
- Active response uses eye shape, head pose, small sound cues, and small visual
  icons before relying on speech.
- When the user is not looking at it, it should stay low-interruption and keep
  quiet autonomous behavior.
- The design question for the hackathon is whether a device that can sense and
  respond to gaze can feel more companion-like than an AI that mainly talks.

Conceptually, the product has four behavior layers:

| Layer | Responsibility | Current repo mapping |
|---|---|---|
| Perception | Face orientation, gaze state, and future ambient audio emotion features | K230 vision firmware now owns face pose; future audio features belong on ESP32/backend depending on latency needs. |
| Behavior | Map user state into internal moods such as curious, relaxed, excited, wronged, or waiting | ESP32 `main/grind_buddy` is the current host-testable behavior boundary. |
| Expression | Drive gimbal motion, eye animation, sound cues, and small icons | ESP32 board/display/audio/motion integration is the target owner. |
| Strategy | Decide when to respond, when to stay quiet, and when speech is appropriate | ESP32 handles local interaction gates; backend handles voice intelligence when speech is used. |

## System Split

```text
K230 CanMV board
  camera, face detection, pose, normalized face observation
        |
        | UART2 binary frames, 921600 baud
        v
ESP32-S3 SZPI board
  display, mic, speaker, Wi-Fi, Xiaozhi protocol, visual wake gate
        |
        | OTA HTTP + Xiaozhi WebSocket voice session
        v
Mac local Xiaozhi-compatible backend
  OTA, WebSocket, Opus voice, ASR, LLM, VLLM, TTS
```

K230 is a vision coprocessor. It does not own audio, display, Wi-Fi, cloud
state, or voice sessions.

ESP32-S3 is the device controller. It owns hardware IO, consumes K230 frames,
invokes Xiaozhi wake flow, and silences listening when vision no longer allows
it.

The Mac backend is the current voice and OTA backend. It is not a business
admin backend in this repo; it is the local Xiaozhi-compatible runtime used by
the bench hardware.

## K230 Module

Location:

```text
firmware/k230/
```

Important files:

- `src/main_vision_uart.py`: CanMV runtime entrypoint.
- `src/transport/vision_protocol.py`: binary frame encoder/decoder and face
  payload format.
- `src/transport/uart_publisher.py`: sequence-owning UART publisher.
- `src/vision/head_pose.py`: K230 face pose detector wrapper.
- `src/vision/visual_observation.py`: normalized observation conversion.
- `tools/build_k230_single.py`: creates `dist/main_vision_uart_single.py`.

The generated single script is tracked intentionally because CanMV deployment
often uses one file.

Protocol:

- magic: `A5 5A`
- version: `1`
- header size: `12`
- max payload: `256`
- CRC: CRC-16/CCITT-FALSE
- messages: heartbeat `1`, face `2`, face-lost `3`, error `127`

The face payload is a normalized, resolution-independent observation:

- `center_x`, `center_y`: `-1000..1000`
- `width`, `height`: `0..1000`
- `pitch`, `yaw`, `roll`: centidegrees
- `confidence`: `0..100`

## ESP32-S3 Module

Location:

```text
firmware/esp32s3/xiaozhi-esp32/
```

This module is a full Xiaozhi ESP-IDF source snapshot.

Important files:

- `main/grind_buddy/k230_binary_protocol.*`: binary K230 parser.
- `main/grind_buddy/k230_vision_adapter.*`: converts binary frames into vision
  events.
- `main/grind_buddy/grind_buddy_controller.*`: debounce, cooldown, and action
  decisions.
- `main/grind_buddy/grind_buddy_uart_integration.*`: UART task and Xiaozhi app
  integration.
- `main/boards/szpi-s3/`: SZPI ESP32-S3 Xiaozhi board profile.
- `main/CMakeLists.txt`, `main/Kconfig.projbuild`, and `main/application.*`:
  Xiaozhi integration points applied directly in the repo snapshot.

Current visual wake behavior:

- `presence.enter` moves the controller to aware state.
- `gaze.enter` opens the listening window after 300 ms of stable frontal gaze.
- stable visual permission for 400 ms starts Xiaozhi wake flow directly.
- wake attempts have a 2000 ms cooldown.
- `gaze.leave` is emitted after 150 ms of stable non-frontal gaze.
- `gaze.leave`, `presence.leave`, or K230 timeout stop voice permission.
- if Xiaozhi is listening while permission is revoked, ESP32 calls
  `Application::SilenceListening()`.

ADR 0001 decides not to reintroduce local VAD gating for the current product
slice. The controller still keeps host-testable local voice detection hooks as
dormant extension points, but the active path uses visual wake directly.

## Local Backend

Location:

```text
backend/xiaozhi-local/
```

The backend subtree preserves the path shape expected by its scripts:

```text
server/xiaozhi-esp32-server/main/xiaozhi-server/
```

Tracked:

- local wrapper scripts under `tools/`
- tests under `tests/`
- docs under `docs/`
- server source under `server/xiaozhi-esp32-server/main/xiaozhi-server/`
- `data/.config.example.yaml`
- `.local.env.example`

Excluded:

- `.local.env`
- generated `data/.config.yaml`
- model weights such as `models/SenseVoiceSmall/model.pt`
- runtime logs and temp files
- ESP-IDF `build*` and `managed_components`
- Python caches and virtual environments
- Node build/dependency directories

Current provider template:

| Layer | Provider | Model / voice |
|---|---|---|
| VAD | `SileroVAD` | local |
| ASR | `FunASR` | `models/SenseVoiceSmall` |
| LLM | `ChatGLMLLM` | `glm-4-flash` |
| VLLM | `ChatGLMVLLM` | `glm-4v-flash` |
| TTS | `EdgeTTS` | `zh-CN-XiaoxiaoNeural` |

## Error Modes

K230 timeout: ESP32 emits `presence.leave` and `gaze.leave` through the adapter
and silences listening if needed.

Corrupt UART data: parser drops bad bytes and recovers at the next magic bytes.

Backend unavailable: ESP32 keeps local firmware running but OTA/voice sessions
cannot progress.

Model missing: backend preflight should fail until
`tools/download_sensevoice_model.sh` has populated the local ignored weight.

Secrets missing: backend config rendering should fail until `.local.env` is
created from `.local.env.example`.
