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
SENSOR_ID_CANDIDATES = (None, 0, 1, 2)


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


class NullFocTrackingPublisher:
    def publish_observation_if_frontal(self, observation):
        return 0

    def publish_face_lost(self):
        return 0


def init_foc_tracking():
    try:
        foc_uart = init_foc_uart()
        return foc_uart, FocTrackingPublisher(foc_uart)
    except Exception as error:
        print("FOC UART disabled:")
        sys.print_exception(error)
        return None, NullFocTrackingPublisher()


def make_pipeline():
    return PipeLine(
        rgb888p_size=FRAME_SIZE,
        display_size=[800, 480],
        display_mode="lcd",
    )


def safe_destroy_pipeline(pipeline):
    if not pipeline:
        return
    try:
        if getattr(pipeline, "sensor", None) is None:
            return
        pipeline.destroy()
    except Exception as error:
        print("pipeline destroy ignored:", error)


def create_pipeline_with_sensor_fallback():
    last_error = None
    for sensor_id in SENSOR_ID_CANDIDATES:
        pipeline = make_pipeline()
        try:
            if sensor_id is None:
                print("Trying camera default sensor")
                pipeline.create()
                print("Camera default sensor initialized")
            else:
                print("Trying camera sensor_id=%d" % sensor_id)
                pipeline.create(sensor_id=sensor_id)
                print("Camera sensor_id=%d initialized" % sensor_id)
            return pipeline
        except Exception as error:
            last_error = error
            if sensor_id is None:
                print("Camera default sensor failed:")
            else:
                print("Camera sensor_id=%d failed:" % sensor_id)
            sys.print_exception(error)
            safe_destroy_pipeline(pipeline)
            gc.collect()
    raise last_error


def run():
    os.exitpoint(os.EXITPOINT_ENABLE)
    uart = init_uart()
    publisher = VisionPublisher(uart)
    foc_uart = None
    foc_publisher = NullFocTrackingPublisher()
    pipeline = None
    detector = None
    last_heartbeat = ticks_ms()
    last_face_lost = -FACE_LOST_REPEAT_MS

    try:
        pipeline = create_pipeline_with_sensor_fallback()
        detector = HeadPoseDetector(
            rgb888p_size=FRAME_SIZE,
            display_size=[800, 480],
            confidence_threshold=0.5,
            nms_threshold=0.2,
        )
        foc_uart, foc_publisher = init_foc_tracking()

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
        safe_destroy_pipeline(pipeline)
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
