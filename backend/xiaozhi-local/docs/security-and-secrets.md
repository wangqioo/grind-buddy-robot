# Security and Secrets

这个仓库只提交可复用工程内容，不提交本机密钥、设备私有配置或模型权重。

## Never Commit

- `server/xiaozhi-esp32-server/data/.config.yaml`
- `server/xiaozhi-esp32-server/data/.memory.yaml`
- `server/xiaozhi-esp32-server/data/.wakeup_words.yaml`
- `server/xiaozhi-esp32-server/main/xiaozhi-server/models/**/model.pt`
- `.env`
- `node_modules/`, `managed_components/`, `build*/`
- generated logs, local sqlite/db files, and Python caches

## API Keys

API key 只放在本机 `.local.env` 和服务器端 `data/.config.yaml`，二者都被 Git 忽略。设备固件只知道 OTA 地址和 WebSocket 地址，不应该持有云服务密钥。

## Device Auth

局域网开发阶段建议开启：

```yaml
server:
  auth:
    enabled: true
    allowed_devices:
      - "AA:BB:CC:DD:EE:FF"
```

这不是官方配对流程，而是自建兼容服的白名单放行机制。

## Before Push

推送前运行：

```bash
tools/verify.sh
find . -name model.pt -o -name .config.yaml -o -name .env -o -name node_modules
rg -n "sk-[A-Za-z0-9]|access_token: \"[A-Za-z0-9_\\-]{10,}|api_key: \"sk-|secret_key: \"[A-Za-z0-9]" .
```
