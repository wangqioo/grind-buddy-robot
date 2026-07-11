# Xiaozhi Local Backend

This directory is the local Xiaozhi-compatible backend used by the Grind Buddy
bench hardware.

It owns:

- OTA response for ESP32-S3.
- Xiaozhi WebSocket voice protocol.
- ASR / LLM / VLLM / TTS provider configuration.
- Local setup, preflight, and smoke-test scripts.

It does not own the ESP32-S3 firmware source. ESP32 work lives at:

```text
/Users/wq/grind-buddy-robot/firmware/esp32s3-main
```

## Runtime Setup

Create local env:

```bash
cd /Users/wq/grind-buddy-robot/backend/xiaozhi-local
cp .local.env.example .local.env
```

Render server config:

```bash
tools/render_local_config.sh
```

Download the local ASR model when needed:

```bash
tools/download_sensevoice_model.sh
```

Run preflight:

```bash
tools/doctor.sh
```

Run server:

```bash
tools/run_server.sh
```

## Git Hygiene

Do not commit:

- `.local.env`
- `server/xiaozhi-esp32-server/data/.config.yaml`
- `server/xiaozhi-esp32-server/data/.memory.yaml`
- `server/xiaozhi-esp32-server/data/.wakeup_words.yaml`
- `server/xiaozhi-esp32-server/main/xiaozhi-server/models/**/model.pt`
- runtime logs, caches, virtual environments, or build output

## Current Provider Template

- VAD: `SileroVAD`
- ASR: `FunASR`, `models/SenseVoiceSmall`
- LLM: `ChatGLMLLM`, `glm-4-flash`
- VLLM: `ChatGLMVLLM`, `glm-4v-flash`
- TTS: `EdgeTTS`, `zh-CN-XiaoxiaoNeural`
