import random
import time
from dataclasses import dataclass
from typing import Callable, Dict, Optional

from core.auth import AuthManager, AuthenticationError


@dataclass
class WebsocketProvision:
    token: str
    activation: Optional[Dict]
    token_required: bool


class DeviceAdmission:
    def __init__(
        self,
        config: dict,
        code_generator: Optional[Callable[[], str]] = None,
        time_fn: Callable[[], float] = time.time,
    ):
        self.config = config
        auth_config = config["server"].get("auth", {})
        self.auth_enable = auth_config.get("enabled", False)
        self.allowed_devices = set(auth_config.get("allowed_devices", []))
        secret_key = config["server"]["auth_key"]
        expire_seconds = auth_config.get("expire_seconds")
        self.auth = AuthManager(secret_key=secret_key, expire_seconds=expire_seconds)
        self._code_generator = code_generator or self._generate_activation_code
        self._time_fn = time_fn
        self._activation_codes: Dict[str, Dict] = {}

    def provision_websocket(
        self, device_id: str, client_id: str, activation_host: str
    ) -> WebsocketProvision:
        if not self.auth_enable:
            return WebsocketProvision(token="", activation=None, token_required=False)

        if self.allowed_devices and device_id not in self.allowed_devices:
            activation_code = self._create_activation_request(device_id)
            return WebsocketProvision(
                token="",
                activation={
                    "code": activation_code,
                    "message": (
                        "Please enter code at http://"
                        + activation_host
                        + "/xiaozhi/admin"
                    ),
                    "timeout_ms": 300000,
                },
                token_required=True,
            )

        token = self.auth.generate_token(client_id, device_id)
        return WebsocketProvision(token=token, activation=None, token_required=True)

    def verify_websocket(
        self, device_id: str, client_id: str, authorization: str
    ) -> bool:
        if not self.auth_enable:
            return True

        if self.allowed_devices and device_id in self.allowed_devices:
            return True

        token = authorization or ""
        if token.startswith("Bearer "):
            token = token[7:]
        else:
            raise AuthenticationError("Missing or invalid Authorization header")

        if self.auth.verify_token(token, client_id=client_id, username=device_id):
            return True

        raise AuthenticationError("Invalid token")

    def verify_activation_code(self, code: str) -> str:
        self._cleanup_expired_codes()
        if code in self._activation_codes:
            mac = self._activation_codes[code]["mac"]
            del self._activation_codes[code]
            return mac
        return ""

    def allow_device(self, device_id: str) -> None:
        self.allowed_devices.add(device_id)
        auth_config = self.config["server"].setdefault("auth", {})
        allowed = set(auth_config.get("allowed_devices", []))
        allowed.add(device_id)
        auth_config["allowed_devices"] = list(allowed)

    def _create_activation_request(self, mac_address: str) -> str:
        self._cleanup_expired_codes()
        code = self._code_generator()
        while code in self._activation_codes:
            code = self._code_generator()

        self._activation_codes[code] = {
            "mac": mac_address,
            "expires": self._time_fn() + 300,
        }
        return code

    def _cleanup_expired_codes(self) -> None:
        now = self._time_fn()
        expired = [c for c, v in self._activation_codes.items() if v["expires"] < now]
        for code in expired:
            del self._activation_codes[code]

    @staticmethod
    def _generate_activation_code() -> str:
        return "".join([str(random.randint(0, 9)) for _ in range(6)])
