import unittest

from src.transport.foc_tracking_publisher import FocTrackingPublisher


class MemoryUART:
    def __init__(self):
        self.writes = []

    def write(self, data):
        self.writes.append(data)
        return len(data)


class FocTrackingPublisherTest(unittest.TestCase):
    def test_ignores_non_frontal_face_before_tracking(self):
        uart = MemoryUART()
        publisher = FocTrackingPublisher(uart)

        publisher.publish_observation_if_frontal(
            {
                "center_x": 200,
                "center_y": -100,
                "pitch_cd": 0,
                "yaw_cd": 4500,
            }
        )

        self.assertEqual(uart.writes, [])

    def test_publishes_center_error_when_frontal(self):
        uart = MemoryUART()
        publisher = FocTrackingPublisher(uart)

        publisher.publish_observation_if_frontal(
            {
                "center_x": 200,
                "center_y": -100,
                "pitch_cd": 500,
                "yaw_cd": -600,
            }
        )

        self.assertEqual(uart.writes, [b"F 200,-100\n"])

    def test_sends_home_once_when_tracking_stops(self):
        uart = MemoryUART()
        publisher = FocTrackingPublisher(uart)

        publisher.publish_observation_if_frontal(
            {
                "center_x": 200,
                "center_y": -100,
                "pitch_cd": 0,
                "yaw_cd": 0,
            }
        )
        publisher.publish_observation_if_frontal(
            {
                "center_x": 300,
                "center_y": -120,
                "pitch_cd": 0,
                "yaw_cd": 5000,
            }
        )
        publisher.publish_observation_if_frontal(
            {
                "center_x": 350,
                "center_y": -150,
                "pitch_cd": 0,
                "yaw_cd": 5000,
            }
        )

        self.assertEqual(uart.writes, [b"F 200,-100\n", b"H\n"])

    def test_sends_home_once_on_face_lost(self):
        uart = MemoryUART()
        publisher = FocTrackingPublisher(uart)

        publisher.publish_observation_if_frontal(
            {
                "center_x": 10,
                "center_y": 20,
                "pitch_cd": 0,
                "yaw_cd": 0,
            }
        )
        publisher.publish_face_lost()
        publisher.publish_face_lost()

        self.assertEqual(uart.writes, [b"F 10,20\n", b"H\n"])


if __name__ == "__main__":
    unittest.main()
