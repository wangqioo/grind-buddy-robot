#!/usr/bin/env python3
import pathlib
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
SDKCONFIG = REPO_ROOT / "sdkconfig"


class DeviceAecConfigTest(unittest.TestCase):
    def test_szpi_s3_build_enables_device_aec_for_barge_in(self):
        sdkconfig = SDKCONFIG.read_text(encoding="utf-8")

        self.assertIn("CONFIG_BOARD_TYPE_SZPI_S3=y", sdkconfig)
        self.assertIn("CONFIG_USE_AUDIO_PROCESSOR=y", sdkconfig)
        self.assertIn("CONFIG_USE_DEVICE_AEC=y", sdkconfig)
        self.assertNotIn("# CONFIG_USE_DEVICE_AEC is not set", sdkconfig)


if __name__ == "__main__":
    unittest.main()
