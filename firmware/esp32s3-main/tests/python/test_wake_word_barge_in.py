#!/usr/bin/env python3
import pathlib
import re
import unittest


REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
APPLICATION_CC = REPO_ROOT / "main" / "application.cc"


class WakeWordBargeInTest(unittest.TestCase):
    def test_wake_word_detection_interrupts_speaking_and_listening(self):
        source = APPLICATION_CC.read_text(encoding="utf-8")
        match = re.search(
            r"void Application::HandleWakeWordDetectedEvent\(\) \{(?P<body>.*?)\n\}",
            source,
            re.S,
        )
        self.assertIsNotNone(match, "HandleWakeWordDetectedEvent not found")
        body = match.group("body")

        self.assertIn(
            "state == kDeviceStateSpeaking || state == kDeviceStateListening",
            body,
        )
        self.assertIn("AbortSpeaking(kAbortReasonWakeWordDetected)", body)
        self.assertIn("audio_service_.PopPacketFromSendQueue()", body)
        self.assertIn("protocol_->SendStartListening", body)
        self.assertIn("audio_service_.ResetDecoder()", body)
        self.assertIn("audio_service_.EnableWakeWordDetection(true)", body)
        self.assertIn("SetListeningMode", body)


if __name__ == "__main__":
    unittest.main()
