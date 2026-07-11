class FocTrackingPublisher:
    def __init__(
        self,
        uart,
        yaw_limit_cd=3000,
        pitch_limit_cd=3000,
    ):
        self.uart = uart
        self.yaw_limit_cd = yaw_limit_cd
        self.pitch_limit_cd = pitch_limit_cd
        self.tracking_active = False

    def publish_observation_if_frontal(self, observation):
        if self._is_frontal(observation):
            self.tracking_active = True
            return self._write_line(
                "F {},{}\n".format(
                    int(observation["center_x"]),
                    int(observation["center_y"]),
                )
            )

        if self.tracking_active:
            return self.publish_face_lost()
        return 0

    def publish_face_lost(self):
        if not self.tracking_active:
            return 0
        self.tracking_active = False
        return self._write_line("H\n")

    def _is_frontal(self, observation):
        return (
            abs(int(observation["yaw_cd"])) <= self.yaw_limit_cd and
            abs(int(observation["pitch_cd"])) <= self.pitch_limit_cd
        )

    def _write_line(self, line):
        data = line.encode("ascii")
        written = self.uart.write(data)
        if written is not None and written != len(data):
            raise OSError("incomplete UART write")
        return len(data)
