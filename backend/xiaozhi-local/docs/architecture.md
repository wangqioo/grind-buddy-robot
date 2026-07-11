# Xiaozhi Local Backend Architecture

This subtree provides the local Xiaozhi-compatible runtime for Grind Buddy.

## Runtime Roles

- ESP32-S3: connects to OTA and Xiaozhi WebSocket endpoints.
- Mac local server: handles OTA, WebSocket control, Opus audio, ASR, LLM, VLLM,
  TTS, and local device admission.
- K230: supplies visual wake information to ESP32-S3 through binary UART; it
  does not talk to this backend directly.
- Cloud APIs: called only by the local server. The device firmware does not
  hold cloud API keys.

## Data Flow

```text
K230 frontal gaze
  -> ESP32-S3 visual wake gate
  -> WebSocket session to local server
  -> ASR
  -> LLM / optional VLLM
  -> TTS
  -> ESP32-S3 speaker
```

## Runtime Seams

### Device Admission

`server/xiaozhi-esp32-server/main/xiaozhi-server/core/device_admission.py`
centralizes local device access:

- `allowed_devices` whitelist.
- OTA token assignment.
- activation code for unbound devices.
- WebSocket bearer token validation.
- in-memory admission updates after local binding.

### OTA Contract

`server/xiaozhi-esp32-server/main/xiaozhi-server/core/ota_contract.py`
centralizes OTA request parsing and response generation:

- device identifiers and firmware metadata
- WebSocket URL fallback from LAN IP and ports
- firmware version comparison
- OTA download URL derivation

### Runtime Profile

`server/xiaozhi-esp32-server/main/xiaozhi-server/config/runtime_profile.py`
normalizes local YAML or manager API config into a `RuntimeProfile`:

- config source
- WebSocket and HTTP public URLs
- auth key source
- selected ASR / LLM / VLLM / TTS / memory / intent modules
- device authentication flags

## Repository Boundary

This subtree is backend-only. ESP32 firmware source, K230 firmware, and hardware
bring-up docs live at the repository root.

Tracked here:

- local backend scripts
- local backend tests
- Xiaozhi server source
- server config template
- backend setup and security docs

Excluded here:

- real `.local.env`
- generated `data/.config.yaml`
- model weights
- logs, caches, virtual environments, build output
- ESP32 firmware worktrees
