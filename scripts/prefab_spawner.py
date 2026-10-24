import random

import engine


class PrefabSpawner(engine.Behaviour):
    """Spawns a prefab around its entity every `interval` seconds.

    All tuning lives in data (ball_spawner.prefab.json or the scene),
    not in this file.
    """

    editor_fields = {
        "prefab": {
            "type": "str",
            "label": "Prefab",
            "default": "ball",
        },
        "interval": {
            "type": "float",
            "label": "Interval (s)",
            "default": 1.0,
            "min": 0.05,
            "max": 10.0,
        },
        "max_alive": {
            "type": "int",
            "label": "Max alive",
            "default": 8,
            "min": 1,
            "max": 100,
        },
        "spread": {
            "type": "float",
            "label": "Spread",
            "default": 3.0,
            "min": 0.0,
            "max": 20.0,
        },
        "height": {
            "type": "float",
            "label": "Height",
            "default": 5.0,
            "min": 0.0,
            "max": 30.0,
        },
        "lifetime": {
            "type": "float",
            "label": "Lifetime override (0 = from prefab)",
            "default": 0.0,
            "min": 0.0,
            "max": 60.0,
        },
    }

    def on_start(self):
        self._timer = 0.0
        self._alive = []

    def on_update(self, dt):
        # Handles go stale when an instance destroys itself
        self._alive = [entity for entity in self._alive if entity.is_valid()]

        self._timer += dt

        if self._timer < self.interval or len(self._alive) >= self.max_alive:
            return

        self._timer = 0.0

        offset = engine.Vec3(
            random.uniform(-self.spread, self.spread),
            self.height,
            random.uniform(-self.spread, self.spread),
        )

        # Per-instance override on top of the template's script fields
        fields = {"lifetime": self.lifetime} if self.lifetime > 0.0 else None

        instance = engine.spawn_prefab(
            self.prefab,
            self.entity.position + offset,
            fields,
        )

        self._alive.append(instance)
