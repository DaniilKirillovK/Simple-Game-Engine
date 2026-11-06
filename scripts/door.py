import engine

from platformer_utils import approach


class Door(engine.Behaviour):
    editor_fields = {
        "open_offset_x": {"type": "float", "label": "Open offset X", "default": 0.0, "min": -50.0, "max": 50.0},
        "open_offset_y": {"type": "float", "label": "Open offset Y", "default": 3.2, "min": -50.0, "max": 50.0},
        "open_offset_z": {"type": "float", "label": "Open offset Z", "default": 0.0, "min": -50.0, "max": 50.0},
        "speed": {"type": "float", "label": "Speed (m/s)", "default": 3.0, "min": 0.1, "max": 50.0},
        "starts_open": {"type": "bool", "label": "Starts open", "default": False},
        "auto_close_time": {
            "type": "float", "label": "Close automatically after (s, 0 = never)",
            "default": 0.0, "min": 0.0, "max": 300.0,
        },
    }

    is_open = False

    def on_start(self):
        self._entity = self.entity
        closed = self._entity.position
        self._closed = (closed.x, closed.y, closed.z)
        self._read_offset()
        self.is_open = self.starts_open
        self._progress = 1.0 if self.starts_open else 0.0
        self._close_timer = 0.0
        self._zero = engine.Vec3(0.0, 0.0, 0.0)
        self.delta = self._zero
        self._entity.position = self._position(self._progress)

    def open(self):
        self.is_open = True
        self._close_timer = self.auto_close_time

    def close(self):
        self.is_open = False
        self._close_timer = 0.0

    def toggle(self):
        if self.is_open:
            self.close()
        else:
            self.open()

    def close_time_left(self):
        if not self.is_open or self.auto_close_time <= 0.0:
            return 0.0
        return self._close_timer

    def _read_offset(self):
        self._offset = (self.open_offset_x, self.open_offset_y, self.open_offset_z)
        self._length = (self._offset[0] ** 2 + self._offset[1] ** 2 + self._offset[2] ** 2) ** 0.5

    def on_field_changed(self, name):
        if name.startswith("open_offset_"):
            self._read_offset()
            self._entity.position = self._position(self._progress)

    def on_update(self, dt):
        if self.is_open and self.auto_close_time > 0.0:
            self._close_timer -= dt
            if self._close_timer <= 0.0:
                self.close()

        target = 1.0 if self.is_open else 0.0
        if self._progress == target:
            self.delta = self._zero
            return

        if self._length > 0.0:
            progress = approach(self._progress, target, self.speed * dt / self._length)
        else:
            progress = target

        step = progress - self._progress
        self._progress = progress
        self._entity.position = self._position(progress)
        self.delta = engine.Vec3(self._offset[0] * step, self._offset[1] * step, self._offset[2] * step)

    def _position(self, progress):
        closed, offset = self._closed, self._offset
        return engine.Vec3(
            closed[0] + offset[0] * progress,
            closed[1] + offset[1] * progress,
            closed[2] + offset[2] * progress,
        )
