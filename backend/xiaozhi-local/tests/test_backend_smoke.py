#!/usr/bin/env python3
import asyncio
import json
import os
import sys
import unittest
from types import SimpleNamespace


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_DIR = os.path.join(
    REPO_ROOT, "server", "xiaozhi-esp32-server", "main", "xiaozhi-server"
)
sys.path.insert(0, SERVER_DIR)

try:
    from core.api.ota_handler import OTAHandler
    from core.websocket_server import WebSocketServer
except ModuleNotFoundError as exc:
    OTAHandler = None
    WebSocketServer = None
    IMPORT_ERROR = exc
else:
    IMPORT_ERROR = None


def make_config():
    return {
        "server": {
            "ip": "127.0.0.1",
            "port": 18001,
            "http_port": 18003,
            "websocket": "ws://127.0.0.1:18001/xiaozhi/v1/",
            "vision_explain": "http://127.0.0.1:18003/mcp/vision/explain",
            "auth_key": "local-auth-key",
            "auth": {
                "enabled": False,
                "allowed_devices": [],
                "expire_seconds": 3600,
            },
        },
        "manager-api": {
            "url": "",
            "secret": "manager-secret",
        },
        "selected_module": {},
    }


class FakeOtaRequest:
    method = "POST"
    host = "127.0.0.1:18003"

    def __init__(self, body):
        self.headers = {
            "device-id": "10:51:db:80:e2:e8",
            "client-id": "smoke-client",
            "device-model": "szpi-s3",
            "device-version": "2.1.0",
        }
        self._body = body

    async def text(self):
        return self._body


class FakeHttpProbe:
    def respond(self, status, text):
        return SimpleNamespace(status=status, text=text)


class BackendSmokeTest(unittest.TestCase):
    def setUp(self):
        if IMPORT_ERROR is not None:
            self.skipTest(f"backend runtime dependency missing: {IMPORT_ERROR.name}")

    def test_ota_post_returns_websocket_route_without_model_runtime(self):
        handler = OTAHandler(make_config())
        request = FakeOtaRequest(
            json.dumps(
                {
                    "board": {"type": "szpi-s3"},
                    "application": {"version": "2.1.0"},
                }
            )
        )

        response = asyncio.run(handler.handle_post(request))
        payload = json.loads(response.text)

        self.assertEqual(response.status, 200)
        self.assertEqual(
            payload["websocket"]["url"],
            "ws://127.0.0.1:18001/xiaozhi/v1/",
        )
        self.assertEqual(payload["websocket"]["token"], "")
        self.assertEqual(payload["firmware"]["version"], "2.1.0")
        self.assertEqual(payload["firmware"]["url"], "")

    def test_websocket_http_probe_does_not_initialize_model_runtime(self):
        request = SimpleNamespace(headers={})

        response = asyncio.run(
            WebSocketServer._http_response(
                SimpleNamespace(),
                FakeHttpProbe(),
                request,
            )
        )

        self.assertEqual(response.status, 200)
        self.assertEqual(response.text, "Server is running\n")


if __name__ == "__main__":
    unittest.main()
