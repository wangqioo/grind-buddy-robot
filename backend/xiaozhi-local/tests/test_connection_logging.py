#!/usr/bin/env python3
import os
import sys
import unittest


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_DIR = os.path.join(
    REPO_ROOT, "server", "xiaozhi-esp32-server", "main", "xiaozhi-server"
)
sys.path.insert(0, SERVER_DIR)

from core.utils.connection_log import is_expected_connection_close, sanitize_headers


class DummyConnectionClosed(Exception):
    pass


class ConnectionLoggingTest(unittest.TestCase):
    def test_sanitize_headers_redacts_auth_tokens_but_keeps_routing_fields(self):
        headers = {
            "host": "192.168.1.26",
            "authorization": "Bearer secret-token",
            "Authorization": "Bearer another-secret",
            "client-id": "client-1",
            "device-id": "10:51:db:80:e2:e8",
            "sec-websocket-key": "raw-websocket-key",
            "x-api-key": "provider-secret",
        }

        sanitized = sanitize_headers(headers)

        self.assertEqual(sanitized["host"], "192.168.1.26")
        self.assertEqual(sanitized["client-id"], "client-1")
        self.assertEqual(sanitized["device-id"], "10:51:db:80:e2:e8")
        self.assertEqual(sanitized["authorization"], "***")
        self.assertEqual(sanitized["Authorization"], "***")
        self.assertEqual(sanitized["sec-websocket-key"], "***")
        self.assertEqual(sanitized["x-api-key"], "***")
        self.assertNotIn("secret-token", repr(sanitized))
        self.assertNotIn("raw-websocket-key", repr(sanitized))

    def test_expected_connection_close_detects_websocket_close_message(self):
        error = DummyConnectionClosed("no close frame received or sent")

        self.assertTrue(is_expected_connection_close(error))

    def test_expected_connection_close_ignores_unrelated_error(self):
        error = RuntimeError("tts provider failed")

        self.assertFalse(is_expected_connection_close(error))


if __name__ == "__main__":
    unittest.main()
