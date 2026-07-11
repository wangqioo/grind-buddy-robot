# Grind Buddy Robot

Grind Buddy Robot is a gaze-aware desktop companion robot built for a
hackathon demo. It is not designed as a conventional voice assistant. The core
idea is that a small robot can feel more alive when it understands whether the
user is looking at it, reacts with expression and body motion first, and only
uses speech when speech is useful.

The current bench build connects three hardware runtimes and a local AI
backend:

- K230 vision coprocessor for fast local face, gaze, and tracking signals.
- ESP32-S3 main controller for screen expression, audio, Wi-Fi, and the
  Xiaozhi-compatible voice session.
- ESP32 FOC controller for gimbal/body motion driven by face-center tracking.
- Mac local backend for ASR, LLM/VLM, TTS, OTA, and WebSocket runtime.

## One-Sentence Demo

When the user turns to look at the robot, K230 detects frontal gaze locally,
ESP32-S3 wakes the interaction, the screen switches emotional expressions, the
FOC body follows the face, and the local backend can answer with short speech or
small sound feedback.

## Why It Is Interesting

Most desktop AI devices are either chat boxes with speakers or screens with an
assistant avatar. Grind Buddy explores a different interaction model:

1. Gaze is the interaction gate. The robot only becomes active when the user is
   looking at it. Looking away closes face tracking and voice permission, so the
   device returns to a quiet default state.
2. Emotion is the state machine. Device states are expressed through animated
   eyes, status text overlays, small sound cues, and gimbal motion instead of
   relying on long spoken replies.
3. Edge and cloud cooperate. K230 handles low-latency local perception, while
   the backend LLM/VLM handles higher-level language and scene understanding.
4. Motion and expression are coupled. Different emotional states can map to both
   screen expressions and FOC body gestures, giving the robot a physical
   response channel beyond speech.
5. The system is modular. Vision, main interaction, motion control, and AI
   backend can be developed and flashed independently.

## Current Verified Status

The current three-firmware bench setup has been run successfully:

- K230 camera detects faces and sends UART data.
- ESP32-S3 receives K230 vision events and opens visual wake only after stable
  frontal gaze.
- ESP32-S3 screen plays exported `emote-assets.bin` animations on a 320x240
  display and overlays status/chat text above the expression.
- ESP32-S3 connects to the local Xiaozhi-compatible backend through OTA and
  WebSocket.
- Backend ASR, LLM, and TTS have been verified independently.
- K230 sends one-way tracking commands to the ESP32 FOC controller.
- ESP32 FOC controller follows `F x,y` face-center offsets and returns home on
  `H`.
- The FOC build no longer depends on the removed BMI160 IMU.

## Architecture

```text
                    local perception
                +----------------------+
                | K230 CanMV camera    |
                | face / gaze / pose   |
                +----------+-----------+
                           |
              +------------+-------------+
              |                          |
  binary UART |                          | text UART
  921600 baud |                          | 115200 baud
              v                          v
+----------------------------+     +--------------------------+
| ESP32-S3 main controller   |     | ESP32 FOC controller     |
| screen / mic / speaker     |     | gimbal / body motion     |
| Wi-Fi / emotion runtime    |     | face-center tracking     |
+-------------+--------------+     +--------------------------+
              |
              | OTA + WebSocket
              v
+----------------------------+
| Mac local backend          |
| ASR / LLM / VLM / TTS      |
| Xiaozhi-compatible runtime |
+----------------------------+
```

The split is intentional:

| Layer | Owner | Responsibility |
|---|---|---|
| Perception | K230 | Local face detection, frontal-gaze gate, tracking offsets. |
| Interaction | ESP32-S3 | Device state, screen expression, audio, Xiaozhi session control. |
| Motion | ESP32 FOC | Local motor loop and smooth body/gimbal tracking. |
| Intelligence | Local backend | ASR, LLM/VLM, TTS, OTA, WebSocket protocol. |

## Interaction Flow

```text
idle/default
  -> user looks at robot
  -> K230 reports stable frontal face
  -> ESP32-S3 enters listening / attentive state
  -> screen switches expression and optional caption overlay
  -> K230 sends tracking offset to FOC controller
  -> FOC moves body toward the face
  -> backend can answer through short speech or sound
  -> user looks away / face lost
  -> ESP32-S3 silences listening permission
  -> K230 sends H to FOC
  -> robot returns to quiet default state
```

This gives the robot a simple but useful social rule: respond when looked at,
stay quiet when ignored.

## Repository Layout

```text
assets/
  emote-assets.bin
    Exported Espressif Emote Gen animation pack used by the ESP32-S3 screen.

backend/xiaozhi-local/
  Local Xiaozhi-compatible backend wrapper, server source, runtime tools,
  config templates, and verification scripts.

firmware/k230-vision/
  CanMV MicroPython vision loop, K230 UART protocol code, tests, and generated
  single-file deployment script.

firmware/esp32s3-main/
  ESP-IDF Xiaozhi firmware snapshot with Grind Buddy display, emotion runtime,
  Wi-Fi, audio, K230 visual wake, and local backend integration.

firmware/esp32-foc/
  PlatformIO ESP32 FOC firmware for gimbal/body motion. Receives `F x,y` and
  `H` commands from K230.

docs/
  Hardware wiring, build/flash notes, architecture notes, expression guide,
  development status, and test matrix.

tools/
  Convenience build and flash helpers.
```

## Key Technical Pieces

### 1. K230 Local Vision Gate

The K230 runs the camera pipeline locally and decides whether a detected face is
frontal enough to activate the robot. This avoids sending every frame to the
cloud and keeps the wake decision fast.

K230 publishes two kinds of outputs:

- Binary face/presence/gaze frames to the ESP32-S3 main controller.
- Lightweight tracking commands to the FOC controller.

FOC command examples:

```text
F center_x,center_y
H
```

`F x,y` is only sent when the primary face is frontal. When the face turns away
or disappears, K230 sends `H` once so the body returns to home/default.

### 2. ESP32-S3 Emotion Display

The ESP32-S3 screen runtime uses an exported Emote Gen asset pack:

```text
assets/emote-assets.bin
```

Current asset pack contains 11 expression animations:

```text
music_25s
sigh_20s_40s
angry_20s
asleep_215s
badminton_12
confident_08
cry_10s_20s
investigate_
laugh_05s_10
leisure_05s_
mock_05s
```

The firmware mounts the `emote_gen` partition, scans all `.eaf` files, reads
the EAF canvas size from the first frame, centers square clips on the 320x240
screen, and keeps status/chat text layered above the animation.

Current provisional state mapping:

| State | Expression candidate |
|---|---|
| idle / neutral | `leisure_05s_` |
| listening / curious | `investigate_` |
| thinking / connecting | `confident_08` |
| speaking / happy | `laugh_05s_10` |
| sleepy | `asleep_215s` |
| sad | `cry_10s_20s` |
| angry / error | `angry_20s` |
| playful | `music_25s` |

See [docs/esp32-emote-gen-guide.md](docs/esp32-emote-gen-guide.md).

### 3. FOC Body Tracking

The FOC controller is deliberately simple from the K230 point of view. It owns
motor control locally and accepts high-level target offsets over UART.

Supported serial commands include:

```text
F x,y       face-center tracking offset
T m0,m1    absolute target degrees relative to power-on home
M0 angle   set vertical axis only
M1 angle   set horizontal axis only
H          return both axes to home
S          print status
?          print help
```

The current firmware does not require the BMI160 IMU. It uses AS5600 encoder
feedback plus serial targets, which matches the current bench hardware.

### 4. Local Xiaozhi-Compatible Backend

The local backend provides the AI runtime and avoids depending on a remote
business server during demo development.

Current provider template:

| Layer | Provider |
|---|---|
| VAD | SileroVAD |
| ASR | FunASR / SenseVoiceSmall |
| LLM | ChatGLMLLM / glm-4-flash |
| VLLM | ChatGLMVLLM / glm-4v-flash |
| TTS | EdgeTTS |

Runtime ports:

```text
8001  Xiaozhi WebSocket
8003  OTA / HTTP endpoints
```

Useful local command:

```bash
cd backend/xiaozhi-local
./tools/doctor.sh
```

`doctor.sh` checks config rendering, model paths, local env, ports, and core
tooling.

## Hardware Wiring

Full wiring notes are in [docs/hardware-wiring.md](docs/hardware-wiring.md).

### K230 to ESP32-S3 Main Controller

```text
K230 GPIO11 UART2_TXD -> ESP32-S3 main RX GPIO10
K230 GPIO12 UART2_RXD <- reserved for future main-controller command channel
GND                   -> GND
Baud                  -> 921600
Protocol              -> binary frames, magic A5 5A, CRC-16/CCITT
```

### K230 to ESP32 FOC Controller

Recommended one-way command wiring:

```text
K230 GPIO3 UART1_TXD -> FOC ESP32 IO16 RX
GND                  -> GND
Baud                 -> 115200
Protocol             -> text commands
```

Keep `FOC ESP32 IO17 TX -> K230 GPIO4 UART1_RXD` disconnected by default. The
FOC link is command-only in the current demo, and the return TX line can disturb
K230 boot if FOC is powered first.

Observed bad-power-order symptom:

```text
RuntimeError: sensor(0) runerror, vicap init failed(-1)
```

Preferred startup:

1. Leave FOC TX disconnected from K230 RX.
2. Share ground between boards.
3. Power K230 and let the camera initialize.
4. Power the FOC board.

## Build And Run

### K230 Vision

```bash
cd firmware/k230-vision
python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
```

Deploy this generated file to the K230 board:

```text
firmware/k230-vision/dist/main_vision_uart_single.py
```

Run on CanMV:

```python
exec(open("/sdcard/pet/main_vision_uart_single.py").read())
```

### ESP32-S3 Main Controller

```bash
cd firmware/esp32s3-main
. /Users/wq/esp-idf/export.sh
idf.py -B build-szpi-s3-local build
idf.py -B build-szpi-s3-local -p /dev/cu.usbmodemXXXX flash
```

Before flashing, identify the chip. The current bench ESP32-S3 main controller
MAC is:

```text
10:51:db:80:e2:e8
```

Expected boot markers:

```text
Project name: xiaozhi
Grind Buddy UART integration started
MMAP Assets [emote_gen] mounted successfully
emote_gen emotion runtime loaded 11 animations
```

### ESP32 FOC Controller

```bash
cd firmware/esp32-foc
pio run
pio run -t upload --upload-port /dev/cu.wchusbserialXXXX
```

Before flashing, identify the chip. The current bench FOC ESP32 MAC is:

```text
1c:c3:ab:27:04:10
```

### Local Backend

```bash
cd backend/xiaozhi-local
./tools/doctor.sh
./tools/run_server.sh
```

The backend uses local ignored config and model files. Public repo templates are
included, but real secrets and model weights are intentionally not committed.

## Demo Checklist For Judges

1. Start the local backend and confirm `8001` and `8003` are listening.
2. Power K230 and confirm the camera initializes.
3. Power ESP32-S3 main controller and wait for Wi-Fi/WebSocket connection.
4. Power the FOC controller.
5. Look at the robot. It should enter the attentive/listening expression state.
6. Move your face while looking at it. The FOC body should track the face-center
   offset.
7. Look away. The robot should close tracking/listening permission and return
   toward default state.
8. Ask a short question. The backend should run ASR -> LLM -> TTS and the
   ESP32-S3 should play the response.

## What Is Closed Loop Now

Closed:

- K230 local gaze gate.
- K230 -> ESP32-S3 visual wake UART.
- K230 -> FOC tracking UART.
- ESP32-S3 expression playback from exported Emote Gen pack.
- ESP32-S3 status/chat text overlay above expression.
- ESP32-S3 -> local backend OTA/WebSocket.
- Local backend ASR, LLM, and TTS.
- FOC tracking without the BMI160 IMU.

Still experimental:

- Final emotion-to-expression mapping.
- Final body gesture library for each emotion.
- Rich cloud VLM scene-understanding policies.
- Long-term memory and proactive companion behavior.
- Fully packaged offline K230 deployment.

## Security And Large Files

This repository intentionally excludes:

- Real backend secrets.
- Generated `.config.yaml`.
- `.local.env`.
- model weights such as SenseVoiceSmall and Silero VAD binaries.
- runtime logs, caches, and build outputs.

Use the provided examples and local setup scripts under
`backend/xiaozhi-local/` to recreate a development machine.

## Useful Docs

- [Architecture](docs/architecture.md)
- [Hardware wiring](docs/hardware-wiring.md)
- [Build and flash](docs/build-and-flash.md)
- [ESP32 Emote Gen integration](docs/esp32-emote-gen-guide.md)
- [Development status](docs/development-status.md)
- [Test matrix](docs/test-matrix.md)

## Repository

GitHub:

```text
https://github.com/wangqioo/grind-buddy-robot
```
