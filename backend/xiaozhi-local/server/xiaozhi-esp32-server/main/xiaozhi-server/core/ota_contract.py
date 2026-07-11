import re
from dataclasses import dataclass
from typing import Dict, Iterable, Optional, Tuple


@dataclass(frozen=True)
class DeviceReport:
    device_id: str
    client_id: str
    model: str
    version: str


def parse_device_report(headers: Dict, body: Dict) -> DeviceReport:
    headers = {str(key).lower(): value for key, value in headers.items()}
    device_id = headers.get("device-id", "")
    client_id = headers.get("client-id", "")
    if not device_id:
        raise ValueError("OTA request device-id is empty")
    if not client_id:
        raise ValueError("OTA request client-id is empty")

    device_model = ""
    for header_name in ("device-model", "device_model", "model"):
        if header_name in headers:
            device_model = headers.get(header_name, "").strip()
            break

    if not device_model:
        if isinstance(body.get("board"), dict):
            device_model = body["board"].get("type", "")
        elif "model" in body:
            device_model = body.get("model", "")
    if not device_model:
        device_model = "default"

    device_version = ""
    for header_name in (
        "device-version",
        "device_version",
        "firmware-version",
        "app-version",
        "application-version",
    ):
        if header_name in headers:
            device_version = headers.get(header_name, "").strip()
            break

    if not device_version and isinstance(body.get("application"), dict):
        device_version = body["application"].get("version", "")
    if not device_version:
        device_version = "0.0.0"

    return DeviceReport(
        device_id=device_id,
        client_id=client_id,
        model=device_model,
        version=device_version,
    )


def resolve_websocket_url(websocket_config: str, local_ip: str, port: int) -> str:
    if websocket_config and "你的" not in websocket_config:
        return websocket_config
    return f"ws://{local_ip}:{port}/xiaozhi/v1/"


def compare_versions(a: str, b: str) -> int:
    ta = _parse_version(a)
    tb = _parse_version(b)
    maxlen = max(len(ta), len(tb))
    for index in range(maxlen):
        ai = ta[index] if index < len(ta) else 0
        bi = tb[index] if index < len(tb) else 0
        if ai > bi:
            return 1
        if ai < bi:
            return -1
    return 0


def choose_firmware_update(
    current_version: str, candidates: Iterable[Tuple[str, str]]
) -> Optional[Tuple[str, str]]:
    sorted_candidates = sorted(
        candidates, key=lambda candidate: _parse_version(candidate[0]), reverse=True
    )
    for version, filename in sorted_candidates:
        if compare_versions(version, current_version) > 0:
            return version, filename
    return None


def build_firmware_download_url(vision_url: str, filename: str) -> str:
    return vision_url.replace(
        "/mcp/vision/explain", f"/xiaozhi/ota/download/{filename}"
    )


def _parse_version(version: str) -> Tuple[int, ...]:
    parts = re.findall(r"\d+", version)
    return tuple(int(part) for part in parts) if parts else (0,)
