"""
K230 visual coprocessor entry point.

Deploy src/ to /sdcard/pet and run:
    exec(open('/sdcard/pet/main_vision_uart.py').read())
"""

import gc
import os
import sys
import time

from machine import FPIOA, UART
from libs.PipeLine import PipeLine

from transport.foc_tracking_publisher import FocTrackingPublisher
from transport.uart_publisher import VisionPublisher
from vision.head_pose import HeadPoseDetector
from vision.visual_observation import make_primary_face_observation


UART_BAUDRATE = 921600
UART_TX_PIN = 11
UART_RX_PIN = 12
FOC_UART_BAUDRATE = 115200
FOC_UART_TX_PIN = 3
FOC_UART_RX_PIN = 4
FRAME_SIZE = [1920, 1080]
HEARTBEAT_INTERVAL_MS = 1000
FACE_LOST_REPEAT_MS = 500


def ticks_ms():
    return time.ticks_ms()


def init_uart():
    fpioa = FPIOA()
    fpioa.set_function(UART_TX_PIN, FPIOA.UART2_TXD)
    fpioa.set_function(UART_RX_PIN, FPIOA.UART2_RXD)
    return UART(UART.UART2, baudrate=UART_BAUDRATE)


def init_foc_uart():
    fpioa = FPIOA()
    fpioa.set_function(FOC_UART_TX_PIN, FPIOA.UART1_TXD)
    fpioa.set_function(FOC_UART_RX_PIN, FPIOA.UART1_RXD)
    return UART(UART.UART1, baudrate=FOC_UART_BAUDRATE)


def run():
    os.exitpoint(os.EXITPOINT_ENABLE)
    uart = init_uart()
    foc_uart = init_foc_uart()
    publisher = VisionPublisher(uart)
    foc_publisher = FocTrackingPublisher(foc_uart)
    pipeline = PipeLine(
        rgb888p_size=FRAME_SIZE,
        display_size=[800, 480],
        display_mode="lcd",
    )
    detector = None
    last_heartbeat = ticks_ms()
    last_face_lost = -FACE_LOST_REPEAT_MS

    try:
        pipeline.create()
        detector = HeadPoseDetector(
            rgb888p_size=FRAME_SIZE,
            display_size=[800, 480],
            confidence_threshold=0.5,
            nms_threshold=0.2,
        )

        while True:
            os.exitpoint()
            now = ticks_ms()
            frame = pipeline.get_frame()
            observation = make_primary_face_observation(
                detector.detect(frame),
                FRAME_SIZE,
            )

            if observation:
                publisher.publish_face_observation(
                    observation,
                    timestamp_ms=now,
                )
                foc_publisher.publish_observation_if_frontal(observation)
            elif time.ticks_diff(now, last_face_lost) >= FACE_LOST_REPEAT_MS:
                publisher.publish_face_lost(now)
                foc_publisher.publish_face_lost()
                last_face_lost = now

            if time.ticks_diff(now, last_heartbeat) >= HEARTBEAT_INTERVAL_MS:
                publisher.publish_heartbeat(now)
                last_heartbeat = now

            gc.collect()
    except Exception as error:
        try:
            publisher.publish_error(1, ticks_ms())
        except Exception:
            pass
        sys.print_exception(error)
        raise
    finally:
        if detector:
            detector.deinit()
        pipeline.destroy()
        try:
            uart.deinit()
        except Exception:
            pass
        try:
            foc_uart.deinit()
        except Exception:
            pass


if __name__ == "__main__":
    run()
