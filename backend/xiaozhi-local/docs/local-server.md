# Local Server

Backend server root:

```text
server/xiaozhi-esp32-server/main/xiaozhi-server
```

This subtree keeps only the local hardware voice-loop pieces needed by Grind
Buddy: upstream server source, config templates, scripts, and tests. Admin
frontends, mobile clients, build products, dependency caches, and firmware
worktrees are outside this backend boundary.

Run the local server from `backend/xiaozhi-local`:

```bash
tools/run_server.sh
```

## Config

Generated runtime config:

```text
server/xiaozhi-esp32-server/data/.config.yaml
```

Template config:

```text
server/xiaozhi-esp32-server/data/.config.example.yaml
```

Prefer editing `.local.env` and regenerating config instead of editing
`.config.yaml` directly:

```bash
tools/render_local_config.sh
```

This keeps LAN IP, ports, device MAC, and API key settings in one local file.

## Ports

- `8001`: Xiaozhi WebSocket voice and control channel.
- `8003`: HTTP OTA endpoint.

The OTA response uses the configured WebSocket URL. For LAN testing, it must be
an address the ESP32-S3 can reach:

```yaml
server:
  websocket: ws://192.168.1.26:8001/xiaozhi/v1/
```

## Verified Local Loop

Expected chain:

```text
device OTA -> server returns websocket config
visual wake -> WebSocket connected
ASR text -> LLM request -> TTS audio -> device playback
```

If OTA works but voice conversation does not, check:

- `server.auth.allowed_devices` contains the ESP32-S3 MAC address.
- `server.websocket` is reachable from the ESP32-S3 LAN.
- Ports `8001` and `8003` are listening.
- The local firewall allows LAN connections.
- Runtime models and provider credentials pass `tools/doctor.sh`.
