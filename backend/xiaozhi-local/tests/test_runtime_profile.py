#!/usr/bin/env python3
import os
import sys
import unittest


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_DIR = os.path.join(
    REPO_ROOT, "server", "xiaozhi-esp32-server", "main", "xiaozhi-server"
)
sys.path.insert(0, SERVER_DIR)

from config.runtime_profile import (
    RuntimeProfile,
    display_http_url,
    display_websocket_url,
    resolve_runtime_profile,
)


def make_config(**overrides):
    config = {
        "server": {
            "ip": "0.0.0.0",
            "port": 8001,
            "http_port": 8003,
            "websocket": "ws://192.168.1.26:8001/xiaozhi/v1/",
            "vision_explain": "http://192.168.1.26:8003/mcp/vision/explain",
            "auth_key": "local-auth-key",
            "auth": {
                "enabled": True,
                "allowed_devices": ["10:51:db:80:e2:e8"],
                "expire_seconds": 3600,
            },
        },
        "manager-api": {
            "url": "",
            "secret": "manager-secret",
        },
        "selected_module": {
            "ASR": "SenseVoice",
            "LLM": "ChatGLMLLM",
            "TTS": "EdgeTTS",
        },
    }
    config.update(overrides)
    return config


class RuntimeProfileTest(unittest.TestCase):
    def test_resolves_local_profile_from_config(self):
        profile = resolve_runtime_profile(make_config())

        self.assertIsInstance(profile, RuntimeProfile)
        self.assertEqual(profile.config_source, "local")
        self.assertEqual(profile.server_ip, "0.0.0.0")
        self.assertEqual(profile.websocket_port, 8001)
        self.assertEqual(profile.http_port, 8003)
        self.assertEqual(profile.websocket_url, "ws://192.168.1.26:8001/xiaozhi/v1/")
        self.assertEqual(
            profile.vision_explain_url,
            "http://192.168.1.26:8003/mcp/vision/explain",
        )
        self.assertEqual(profile.auth_key, "local-auth-key")
        self.assertEqual(profile.allowed_devices, ("10:51:db:80:e2:e8",))
        self.assertEqual(profile.selected_modules["LLM"], "ChatGLMLLM")

    def test_resolves_manager_profile_source_when_api_config_is_loaded(self):
        profile = resolve_runtime_profile(make_config(read_config_from_api=True))

        self.assertEqual(profile.config_source, "manager-api")

    def test_auth_key_falls_back_to_manager_secret(self):
        config = make_config()
        config["server"]["auth_key"] = "你的auth_key"

        profile = resolve_runtime_profile(config)

        self.assertEqual(profile.auth_key, "manager-secret")
        self.assertEqual(config["server"]["auth_key"], "manager-secret")

    def test_auth_key_uses_generated_fallback_when_local_and_manager_are_placeholders(self):
        config = make_config()
        config["server"]["auth_key"] = ""
        config["manager-api"]["secret"] = "你的server.secret值"

        profile = resolve_runtime_profile(config, fallback_auth_key=lambda: "generated")

        self.assertEqual(profile.auth_key, "generated")
        self.assertEqual(config["server"]["auth_key"], "generated")

    def test_missing_server_values_use_existing_defaults(self):
        config = make_config(server={"auth_key": "local-auth-key"})

        profile = resolve_runtime_profile(config)

        self.assertEqual(profile.server_ip, "0.0.0.0")
        self.assertEqual(profile.websocket_port, 8000)
        self.assertEqual(profile.http_port, 8003)
        self.assertEqual(profile.websocket_url, "")
        self.assertEqual(profile.vision_explain_url, "")

    def test_display_urls_prefer_configured_lan_urls_over_detected_local_ip(self):
        profile = resolve_runtime_profile(make_config())

        self.assertEqual(
            display_http_url(profile, detected_local_ip="198.18.0.1"),
            "http://192.168.1.26:8003/mcp/vision/explain",
        )
        self.assertEqual(
            display_websocket_url(profile, detected_local_ip="198.18.0.1"),
            "ws://192.168.1.26:8001/xiaozhi/v1/",
        )

    def test_display_urls_fall_back_when_configured_urls_are_missing(self):
        config = make_config(server={"auth_key": "local-auth-key"})
        profile = resolve_runtime_profile(config)

        self.assertEqual(
            display_http_url(profile, detected_local_ip="10.0.0.5"),
            "http://10.0.0.5:8003/mcp/vision/explain",
        )
        self.assertEqual(
            display_websocket_url(profile, detected_local_ip="10.0.0.5"),
            "ws://10.0.0.5:8000/xiaozhi/v1/",
        )


if __name__ == "__main__":
    unittest.main()
