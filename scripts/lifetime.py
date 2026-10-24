import engine


class Lifetime(engine.Behaviour):
    """Destroys its entity after `lifetime` seconds. The value comes from the prefab."""

    editor_fields = {
        "lifetime": {
            "type": "float",
            "label": "Lifetime (s)",
            "default": 5.0,
            "min": 0.1,
            "max": 60.0,
        },
    }

    def on_start(self):
        self._age = 0.0

    def on_update(self, dt):
        self._age += dt

        if self._age >= self.lifetime:
            self.entity.destroy()
