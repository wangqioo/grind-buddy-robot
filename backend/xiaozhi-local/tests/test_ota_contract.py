#!/usr/bin/env python3
import os
import sys
import unittest


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_DIR = os.path.join(
    REPO_ROOT, "server", "xiaozhi-esp32-server", "main", "xiaozhi-server"
)
sys.path.insert(0, SERVER_DIR)

from core.ota_contract import (
    DeviceReport,
    build_firmware_download_url,
    choose_firmware_update,
    compare_versions,
    parse_device_report,
    resolve_websocket_url,
)


class OTAContractTest(unittest.TestCase):
    def test_parse_device_report_prefers_headers(self):
        report = parse_device_report(
            headers={
                "device-id": "AA:BB",
                "client-id": "client-1",
                "device-model": "szpi-s3",
                "device-version": "1.2.3",
            },
            body={"board": {"type": "body-model"}, "application": {"version": "0.0.1"}},
        )

        self.assertEqual(
            report,
            DeviceReport(
                device_id="AA:BB",
                client_id="client-1",
                model="szpi-s3",
                version="1.2.3",
            ),
        )

    def test_parse_device_report_treats_header_names_case_insensitively(self):
        report = parse_device_report(
            headers={
                "Device-ID": "AA:BB",
                "Client-ID": "client-1",
                "Device-Model": "szpi-s3",
                "Firmware-Version": "2.0.0",
            },
            body={},
        )

        self.assertEqual(report.device_id, "AA:BB")
        self.assertEqual(report.client_id, "client-1")
        self.assertEqual(report.model, "szpi-s3")
        self.assertEqual(report.version, "2.0.0")

    def test_parse_device_report_uses_body_fallbacks_and_defaults(self):
        report = parse_device_report(
            headers={"device-id": "AA:BB", "client-id": "client-1"},
            body={"board": {"type": "body-model"}},
        )

        self.assertEqual(report.model, "body-model")
        self.assertEqual(report.version, "0.0.0")

    def test_parse_device_report_requires_device_and_client_ids(self):
        with self.assertRaises(ValueError):
            parse_device_report(headers={"device-id": "AA:BB"}, body={})

    def test_resolve_websocket_url_uses_configured_url_when_real(self):
        self.assertEqual(
            resolve_websocket_url(
                websocket_config="ws://192.168.1.26:8001/xiaozhi/v1/",
                local_ip="10.0.0.1",
                port=8001,
            ),
            "ws://192.168.1.26:8001/xiaozhi/v1/",
        )

    def test_resolve_websocket_url_falls_back_for_placeholder(self):
        self.assertEqual(
            resolve_websocket_url(
                websocket_config="ws://你的ip或者域名:端口号/xiaozhi/v1/",
                local_ip="10.0.0.1",
                port=18001,
            ),
            "ws://10.0.0.1:18001/xiaozhi/v1/",
        )

    def test_compare_versions_uses_numeric_parts(self):
        self.assertGreater(compare_versions("1.10.0", "1.2.9"), 0)
        self.assertEqual(compare_versions("1.0", "1.0.0"), 0)
        self.assertLess(compare_versions("0.9.9", "1.0.0"), 0)

    def test_choose_firmware_update_returns_newest_higher_version(self):
        update = choose_firmware_update(
            current_version="1.0.0",
            candidates=[("1.1.0", "szpi-s3_1.1.0.bin"), ("1.2.0", "szpi-s3_1.2.0.bin")],
        )

        self.assertEqual(update, ("1.2.0", "szpi-s3_1.2.0.bin"))

    def test_choose_firmware_update_returns_none_when_current_is_latest(self):
        self.assertIsNone(
            choose_firmware_update(
                current_version="1.2.0",
                candidates=[("1.1.0", "szpi-s3_1.1.0.bin")],
            )
        )

    def test_build_firmware_download_url_replaces_vision_path(self):
        self.assertEqual(
            build_firmware_download_url(
                "http://192.168.1.26:8003/mcp/vision/explain",
                "szpi-s3_1.2.0.bin",
            ),
            "http://192.168.1.26:8003/xiaozhi/ota/download/szpi-s3_1.2.0.bin",
        )


if __name__ == "__main__":
    unittest.main()
