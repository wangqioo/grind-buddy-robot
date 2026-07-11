#!/usr/bin/env python3
import os
import sys
import time
import unittest


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_DIR = os.path.join(
    REPO_ROOT, "server", "xiaozhi-esp32-server", "main", "xiaozhi-server"
)
sys.path.insert(0, SERVER_DIR)

from core.auth import AuthenticationError
from core.device_admission import DeviceAdmission


def make_config(allowed_devices=None, auth_enabled=True, expire_seconds=3600):
    return {
        "server": {
            "auth_key": "test-secret",
            "auth": {
                "enabled": auth_enabled,
                "allowed_devices": allowed_devices or [],
                "expire_seconds": expire_seconds,
            },
        }
    }


class DeviceAdmissionTest(unittest.TestCase):
    def test_allowed_device_receives_websocket_token(self):
        admission = DeviceAdmission(make_config(["AA:BB:CC:DD:EE:FF"]))

        result = admission.provision_websocket(
            device_id="AA:BB:CC:DD:EE:FF",
            client_id="client-1",
            activation_host="localhost:8003",
        )

        self.assertEqual(result.token_required, True)
        self.assertNotEqual(result.token, "")
        self.assertIsNone(result.activation)
        self.assertTrue(
            admission.verify_websocket(
                device_id="AA:BB:CC:DD:EE:FF",
                client_id="client-1",
                authorization=f"Bearer {result.token}",
            )
        )

    def test_unlisted_device_receives_activation_request_without_token(self):
        admission = DeviceAdmission(
            make_config(["AA:BB:CC:DD:EE:FF"]), code_generator=lambda: "123456"
        )

        result = admission.provision_websocket(
            device_id="11:22:33:44:55:66",
            client_id="client-2",
            activation_host="localhost:8003",
        )

        self.assertEqual(result.token, "")
        self.assertEqual(
            result.activation,
            {
                "code": "123456",
                "message": "Please enter code at http://localhost:8003/xiaozhi/admin",
                "timeout_ms": 300000,
            },
        )

    def test_invalid_token_raises_authentication_error(self):
        admission = DeviceAdmission(make_config([]))

        with self.assertRaises(AuthenticationError):
            admission.verify_websocket(
                device_id="AA:BB:CC:DD:EE:FF",
                client_id="client-1",
                authorization="Bearer invalid-token",
            )

    def test_activation_code_is_consumed_once(self):
        admission = DeviceAdmission(
            make_config(["AA:BB:CC:DD:EE:FF"]), code_generator=lambda: "654321"
        )
        admission.provision_websocket(
            device_id="11:22:33:44:55:66",
            client_id="client-2",
            activation_host="localhost:8003",
        )

        self.assertEqual(admission.verify_activation_code("654321"), "11:22:33:44:55:66")
        self.assertEqual(admission.verify_activation_code("654321"), "")

    def test_expired_activation_code_is_rejected(self):
        now = [1000.0]
        admission = DeviceAdmission(
            make_config(["AA:BB:CC:DD:EE:FF"]),
            code_generator=lambda: "111111",
            time_fn=lambda: now[0],
        )
        admission.provision_websocket(
            device_id="11:22:33:44:55:66",
            client_id="client-2",
            activation_host="localhost:8003",
        )

        now[0] = 1000.0 + 301.0

        self.assertEqual(admission.verify_activation_code("111111"), "")

    def test_auth_disabled_allows_websocket_without_token(self):
        admission = DeviceAdmission(make_config(auth_enabled=False))

        result = admission.provision_websocket(
            device_id="AA:BB:CC:DD:EE:FF",
            client_id="client-1",
            activation_host="localhost:8003",
        )

        self.assertEqual(result.token, "")
        self.assertIsNone(result.activation)
        self.assertTrue(
            admission.verify_websocket(
                device_id="AA:BB:CC:DD:EE:FF",
                client_id="client-1",
                authorization="",
            )
        )


if __name__ == "__main__":
    unittest.main()
