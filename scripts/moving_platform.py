import engine


class MovingPlatform(engine.Behaviour):
    editor_fields = {
        "offset_x": {"type": "float", "label": "Offset X", "default": 0.0, "min": -100.0, "max": 100.0},
        "offset_y": {"type": "float", "label": "Offset Y", "default": 4.0, "min": -100.0, "max": 100.0},
        "offset_z": {"type": "float", "label": "Offset Z", "default": 0.0, "min": -100.0, "max": 100.0},
        "speed": {"type": "float", "label": "Speed (m/s)", "default": 2.0, "min": 0.1, "max": 50.0},
        "wait_time": {"type": "float", "label": "Wait at ends (s)", "default": 1.0, "min": 0.0, "max": 60.0},
        "start_delay": {"type": "float", "label": "Start delay (s)", "default": 0.0, "min": 0.0, "max": 60.0},
    }

    def on_start(self):
        self._entity = self.entity
        start = self._entity.position
        self._start = (start.x, start.y, start.z)
        self._read_offset()
        self._progress = 0.0
        self._direction = 1.0
        self._wait = self.start_delay
        self._zero = engine.Vec3(0.0, 0.0, 0.0)
        self.delta = self._zero

    def _read_offset(self):
        self._offset = (self.offset_x, self.offset_y, self.offset_z)
        self._length = (self.offset_x ** 2 + self.offset_y ** 2 + self.offset_z ** 2) ** 0.5

    def on_field_changed(self, name):
        if name.startswith("offset_"):
            self._read_offset()

    def on_update(self, dt):
        self.delta = self._zero

        if self._length <= 0.0:
            return

        if self._wait > 0.0:
            self._wait -= dt
            return

        progress = self._progress + self._direction * self.speed * dt / self._length

        if progress >= 1.0:
            progress = 1.0
            self._direction = -1.0
            self._wait = self.wait_time
        elif progress <= 0.0:
            progress = 0.0
            self._direction = 1.0
            self._wait = self.wait_time

        step = progress - self._progress
        self._progress = progress

        start, offset = self._start, self._offset
        self._entity.position = engine.Vec3(
            start[0] + offset[0] * progress,
            start[1] + offset[1] * progress,
            start[2] + offset[2] * progress,
        )
        self.delta = engine.Vec3(offset[0] * step, offset[1] * step, offset[2] * step)
