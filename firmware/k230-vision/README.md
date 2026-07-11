# K230 Vision Coprocessor

This directory contains the active K230 CanMV vision firmware for Grind Buddy.
The runtime target is a single generated MicroPython file:

```text
dist/main_vision_uart_single.py
```

## Build

Regenerate the deployable single-file script after editing anything under
`src/`:

```bash
cd /Users/wq/grind-buddy-ai-hardware/firmware/k230
python3 tools/build_k230_single.py
```

The generated file is import-free and bundles:

- `src/transport/vision_protocol.py`
- `src/transport/uart_publisher.py`
- `src/vision/visual_observation.py`
- `src/vision/head_pose.py`
- `src/main_vision_uart.py`

## Deploy

Copy the generated file to the K230 board:

```text
/sdcard/pet/main_vision_uart_single.py
```

Run it from the CanMV REPL:

```python
exec(open("/sdcard/pet/main_vision_uart_single.py").read())
```

The script initializes the LCD camera pipeline, runs head-pose detection, and
publishes vision state to ESP32-S3 over UART.

## UART Contract

Physical wiring:

- K230 UART2 TX: `GPIO11`
- K230 UART2 RX: `GPIO12` reserved for future commands
- ESP32-S3 UART1 RX: `GPIO10`
- baud: `921600`

Frame format:

- magic: `A5 5A`
- version: `1`
- CRC: CRC-16/CCITT-FALSE over header and payload
- maximum payload: 256 bytes

Message types:

| Type | Name | Payload |
|---:|---|---|
| `1` | heartbeat | empty |
| `2` | face | normalized face observation |
| `3` | face lost | empty |
| `127` | error | 2-byte error code |

Face payload values are normalized in `src/transport/vision_protocol.py`:

- center x/y: `-1000..1000`
- width/height: `0..1000`
- pitch/yaw/roll: centi-degrees
- confidence: `0..100`

## Local Verification

Run before deployment:

```bash
cd /Users/wq/grind-buddy-ai-hardware/firmware/k230
python3 tools/build_k230_single.py
python3 -m unittest discover -s tests -p 'test_*.py' -v
python3 -m compileall src tools
```

Expected result:

- the generated single script exists
- all Python tests pass
- source files compile under local Python

## Hardware Verification

After deployment:

1. Start the local Xiaozhi backend and confirm `tools/doctor.sh` is ready.
2. Boot the ESP32-S3 Grind Buddy firmware.
3. Run the K230 script from CanMV.
4. Confirm ESP32 logs include:

```text
Grind Buddy UART integration started: uart=1 rx=10 baud=921600
```

5. Look directly at the K230 camera. Expected behavior:

- K230 publishes type `2` face frames.
- ESP32 opens Xiaozhi listening after stable frontal gaze.
- Turning away or leaving frame publishes face-lost behavior and closes
  listening silently.

If no visual wake occurs, check wiring first: K230 `GPIO11` must reach ESP32-S3
`GPIO10`, with common ground.

## Debug Examples

CanMV-only debug scripts belong under `examples/`. They are not bundled into the
runtime single-file script.
