#!/usr/bin/env python3
import os
import sys
import unittest
from types import SimpleNamespace


REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SERVER_DIR = os.path.join(
    REPO_ROOT, "server", "xiaozhi-esp32-server", "main", "xiaozhi-server"
)
sys.path.insert(0, SERVER_DIR)

from core.utils.voice_latency import (  # noqa: E402
    VOICE_LATENCY_PREFIX,
    mark_voice_latency,
    mark_voice_latency_once,
    reset_voice_latency,
)


class FakeLogger:
    def __init__(self):
        self.messages = []

    def bind(self, **_kwargs):
        return self

    def info(self, message):
        self.messages.append(message)


class VoiceLatencyTest(unittest.TestCase):
    def test_reset_and_mark_emit_total_and_delta_seconds(self):
        logger = FakeLogger()
        conn = SimpleNamespace(
            logger=logger,
            session_id="session-1",
            sentence_id="sentence-1",
        )

        reset_voice_latency(conn, "listen_start", detail="mode=auto", now=10.0)
        mark_voice_latency(conn, "asr_done", detail="text=hello", now=12.5)

        self.assertEqual(len(logger.messages), 2)
        self.assertIn(VOICE_LATENCY_PREFIX, logger.messages[0])
        self.assertIn("event=listen_start", logger.messages[0])
        self.assertIn("total=0.000s", logger.messages[0])
        self.assertIn("delta=0.000s", logger.messages[0])
        self.assertIn("detail=mode=auto", logger.messages[0])
        self.assertIn("event=asr_done", logger.messages[1])
        self.assertIn("total=2.500s", logger.messages[1])
        self.assertIn("delta=2.500s", logger.messages[1])
        self.assertIn("session=session-1", logger.messages[1])
        self.assertIn("sentence=sentence-1", logger.messages[1])

    def test_mark_once_suppresses_duplicate_events(self):
        logger = FakeLogger()
        conn = SimpleNamespace(logger=logger, session_id="session-1")

        reset_voice_latency(conn, "listen_start", now=1.0)
        mark_voice_latency_once(conn, "llm_first_token", now=2.0)
        mark_voice_latency_once(conn, "llm_first_token", now=3.0)

        matching = [
            message for message in logger.messages if "event=llm_first_token" in message
        ]
        self.assertEqual(len(matching), 1)


if __name__ == "__main__":
    unittest.main()
