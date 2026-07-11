# Quick Start

This flow brings up the local Xiaozhi-compatible backend for the Grind Buddy
bench hardware. ESP32-S3 firmware work lives in
`firmware/esp32s3/xiaozhi-esp32`; this directory only owns the backend runtime.

## 1. Prepare Config

From the repository root:

```bash
cd backend/xiaozhi-local
cp .local.env.example .local.env
```

Edit `.local.env` with local values:

- `XIAOZHI_LAN_IP`: LAN IP reachable by the ESP32-S3.
- `XIAOZHI_DEVICE_MAC`: ESP32-S3 device MAC address.
- `XIAOZHI_GLM_API_KEY`: Zhipu BigModel-compatible API key.
- `XIAOZHI_WS_PORT`: WebSocket port, default `8001`.
- `XIAOZHI_HTTP_PORT`: HTTP OTA port, default `8003`.

Render the server config:

```bash
tools/render_local_config.sh
```

For one-command setup when `GLM_API_KEY` or `XIAOZHI_GLM_API_KEY` is already
exported:

```bash
tools/prepare_local_runtime.sh
```

Real config files and secrets are ignored by Git and must stay local.

## 2. Install Runtime Assets

```bash
tools/download_sensevoice_model.sh
tools/setup_server_env.sh
tools/sync_silero_vad_model.sh
```

On Mac arm64, setup uses `requirements-macos-arm64.txt` when appropriate.
`tools/sync_silero_vad_model.sh` copies `silero_vad.jit` from the installed
conda package into the path expected by the upstream Xiaozhi server.

## 3. Run Preflight

```bash
tools/doctor.sh
```

Preflight checks local config, placeholder values, SenseVoice, Silero VAD,
conda, ffmpeg, common ports, and ESP-IDF. Fix `FAIL` results before runtime.
`WARN` usually means a service is not running yet or an optional local
dependency is absent.

For lightweight source checks after script or doc changes:

```bash
tools/verify.sh
```

## 4. Run Server

```bash
tools/run_server.sh
```

Expected ports:

- WebSocket: `8001`
- HTTP OTA: `8003`

Check ports:

```bash
tools/check_ports.sh
```

## 5. Configure ESP32-S3 Firmware

Generate local ESP-IDF defaults so OTA points at the backend:

```bash
LAN_IP=<backend-lan-ip> tools/render_local_config.sh \
  --firmware-defaults ../../firmware/esp32s3/xiaozhi-esp32/sdkconfig.defaults.local
```

Then build and flash the ESP32-S3 firmware from
`firmware/esp32s3/xiaozhi-esp32`. See the repository root `README.md` and
`firmware/esp32s3/README.md`.

## 6. Monitor Runtime

After ESP32-S3 joins the LAN, OTA should return:

```text
ws://<backend-lan-ip>:8001/xiaozhi/v1/
```

Expected runtime chain:

```text
K230 frontal gaze -> ESP32-S3 visual wake -> WebSocket connect -> ASR -> LLM -> TTS -> speaker playback
```
