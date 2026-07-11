import uuid
from dataclasses import dataclass
from typing import Callable, Dict, Tuple


@dataclass(frozen=True)
class RuntimeProfile:
    config_source: str
    server_ip: str
    websocket_port: int
    http_port: int
    websocket_url: str
    vision_explain_url: str
    auth_enabled: bool
    auth_key: str
    allowed_devices: Tuple[str, ...]
    selected_modules: Dict


def resolve_runtime_profile(
    config: dict, fallback_auth_key: Callable[[], str] = None
) -> RuntimeProfile:
    server_config = config.setdefault("server", {})
    manager_config = config.get("manager-api", {})
    auth_config = server_config.setdefault("auth", {})

    fallback_auth_key = fallback_auth_key or (lambda: uuid.uuid4().hex)
    auth_key = _resolve_auth_key(server_config, manager_config, fallback_auth_key)
    server_config["auth_key"] = auth_key

    return RuntimeProfile(
        config_source="manager-api" if config.get("read_config_from_api", False) else "local",
        server_ip=server_config.get("ip", "0.0.0.0") or "0.0.0.0",
        websocket_port=int(server_config.get("port", 8000) or 8000),
        http_port=int(server_config.get("http_port", 8003) or 8003),
        websocket_url=server_config.get("websocket", "") or "",
        vision_explain_url=server_config.get("vision_explain", "") or "",
        auth_enabled=bool(auth_config.get("enabled", False)),
        auth_key=auth_key,
        allowed_devices=tuple(auth_config.get("allowed_devices", []) or []),
        selected_modules=dict(config.get("selected_module", {}) or {}),
    )


def display_http_url(profile: RuntimeProfile, detected_local_ip: str) -> str:
    if _is_real_value(profile.vision_explain_url):
        return profile.vision_explain_url
    return f"http://{detected_local_ip}:{profile.http_port}/mcp/vision/explain"


def display_websocket_url(profile: RuntimeProfile, detected_local_ip: str) -> str:
    if _is_real_value(profile.websocket_url):
        return profile.websocket_url
    return f"ws://{detected_local_ip}:{profile.websocket_port}/xiaozhi/v1/"


def _resolve_auth_key(
    server_config: dict, manager_config: dict, fallback_auth_key: Callable[[], str]
) -> str:
    auth_key = server_config.get("auth_key", "")
    if _is_real_value(auth_key):
        return auth_key

    manager_secret = manager_config.get("secret", "")
    if _is_real_value(manager_secret):
        return manager_secret

    return fallback_auth_key()


def _is_real_value(value) -> bool:
    if not value:
        return False
    value = str(value)
    return "你" not in value and "YOUR_" not in value
