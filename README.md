# Grind Buddy Robot

Grind Buddy Robot is the integrated hardware project for the gaze-responsive desktop companion robot.

This repository contains the currently working code for:

- K230 CanMV vision coprocessor
- ESP32-S3 main controller with screen, voice, Wi-Fi, and emotion display
- ESP32 FOC gimbal/body controller
- Local Xiaozhi-compatible backend

## Runtime Chain

```text
K230 camera
  -> ESP32-S3 main controller: binary vision UART, visual wake, display, audio
  -> ESP32 FOC controller: face-center tracking UART, body motion
ESP32-S3 main controller
  -> local backend: ASR, LLM/VLM, TTS, OTA/WebSocket
```

## Layout

```text
assets/
  emote-assets.bin

firmware/k230-vision/
  CanMV MicroPython vision loop and generated single-file deployment script.

firmware/esp32s3-main/
  ESP-IDF Xiaozhi firmware snapshot with Grind Buddy display, emotion, Wi-Fi,
  visual wake, and K230 integration.

firmware/esp32-foc/
  PlatformIO ESP32 FOC gimbal firmware. Receives `F x,y` and `H` from K230.

backend/xiaozhi-local/
  Local Xiaozhi-compatible backend wrapper and server code.

tools/
  Build and flash helper scripts.
```

## Hardware Wiring

See [docs/hardware-wiring.md](docs/hardware-wiring.md).

Important FOC wiring note: keep the K230-to-FOC UART one-way by default:
`K230 GPIO3 UART1_TXD -> FOC ESP32 IO16 RX`, plus common ground. Do not connect
`FOC ESP32 IO17 TX -> K230 GPIO4 UART1_RXD` unless proper isolation/buffering is
added. With FOC powered first, that return TX line can disturb K230 boot and
cause `RuntimeError: sensor(0) runerror, vicap init failed(-1)` during camera
startup.

## Common Commands

```bash
# K230: regenerate deployable single file
tools/build-k230.sh

# ESP32-S3 main controller
tools/build-esp32s3.sh

# FOC controller
tools/build-foc.sh
tools/flash-foc.sh /dev/cu.wchusbserial1110
```

## Deployment Files

K230 deploy file:

```text
firmware/k230-vision/dist/main_vision_uart_single.py
```

Run on CanMV:

```python
exec(open("/sdcard/pet/main_vision_uart_single.py").read())
```

ESP32-S3 main firmware output:

```text
firmware/esp32s3-main/build-szpi-s3-local/xiaozhi.bin
```

FOC firmware output:

```text
firmware/esp32-foc/.pio/build/lolin32_lite/firmware.bin
```

## Notes

- `assets/emote-assets.bin` is included so the ESP32-S3 display build does not depend on `/Users/wq/Downloads`.
- Backend secrets, model weights, runtime data, and build outputs are intentionally not included.
- The current bench Wi-Fi defaults are in `firmware/esp32s3-main/sdkconfig.defaults.local`.
