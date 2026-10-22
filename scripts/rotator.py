import engine


class Rotator(engine.Behaviour):
    editor_fields = {
        "speed": {
            "type": "float",
            "label": "Speed",
            "default": 90.0,
            "min": 0.0,
            "max": 360.0,
        },
        "enabled": {
            "type": "bool",
            "label": "Enabled",
            "default": True,
        },
        "axis": {
            "type": "enum",
            "label": "Axis",
            "default": "Y",
            "options": ["X", "Y", "Z"],
        },
        "target": {
            "type": "entity",
            "label": "Target",
            "default": None,
        },
        "destroy_after": {
            "type": "float",
            "label": "Destroy target after (0 = never)",
            "default": 0.0,
            "min": 0.0,
            "max": 30.0,
        },
    }

    def on_start(self):
        self._elapsed = 0.0
        self._had_target = self.target is not None
        self._destroy_requested = False

        target = self.target

        if target is None:
            engine.log("Rotator started without a target")
        else:
            engine.log(
                f"Rotator started: target={target.name}, "
                f"id={target.id}"
            )

    def on_update(self, dt):
        target = self.target

        if target is None:
            if self._had_target:
                engine.log(
                    "Rotator: target was destroyed; "
                    "self.target is now None"
                )
                self._had_target = False

            return

        self._had_target = True

        if not self.enabled:
            return

        rotation = target.rotation
        angle = self.speed * dt

        if self.axis == "X":
            rotation.x += angle
        elif self.axis == "Y":
            rotation.y += angle
        else:
            rotation.z += angle

        target.rotation = rotation

        self._elapsed += dt

        if (
            self.destroy_after > 0.0
            and not self._destroy_requested
            and self._elapsed >= self.destroy_after
        ):
            engine.log(
                f"Rotator: requesting destruction of "
                f"target id={target.id}"
            )

            target.destroy()
            self._destroy_requested = True

    def on_destroy(self):
        engine.log("Rotator stopped")