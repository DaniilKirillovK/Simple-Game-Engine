import engine

from platformer_utils import approach


class ShootTarget(engine.Behaviour):
    editor_fields = {
        "target": {"type": "entity", "label": "Target (door)", "default": None},
        "action": {
            "type": "enum", "label": "Action when knocked", "default": "open",
            "options": ["none", "open", "close", "toggle"],
        },
        "hits_required": {"type": "int", "label": "Hits required", "default": 1, "min": 1, "max": 100},
        "reset_time": {
            "type": "float", "label": "Stand up again after (s, 0 = never)",
            "default": 0.0, "min": 0.0, "max": 300.0,
        },
        "score": {"type": "int", "label": "Score", "default": 100, "min": 0, "max": 100000},
    }

    knocked = False

    def on_start(self):
        self._entity = self.entity
        self.knocked = False
        self._hits = 0
        self._timer = 0.0
        self._angle = 0.0
        self._flash = 0.0
        rotation = self._entity.rotation
        scale = self._entity.scale
        self._base_rotation = (rotation.x, rotation.y, rotation.z)
        self._base_scale = (scale.x, scale.y, scale.z)

    def on_shot(self, damage, point):
        if self.knocked:
            return

        self._hits += damage
        self._flash = 1.0

        if self._hits >= self.hits_required:
            self._knock()

    def _knock(self):
        self.knocked = True
        self._timer = self.reset_time

        target = self.target
        door = target.behaviour if target is not None else None
        if door is not None:
            if self.action == "open":
                door.open()
            elif self.action == "close":
                door.close()
            elif self.action == "toggle":
                door.toggle()

        game = engine.find("Game")
        director = game.behaviour if game is not None else None
        if director is not None and hasattr(director, "add_score"):
            director.add_score(self.score)

    def on_update(self, dt):
        if self.knocked and self.reset_time > 0.0:
            self._timer -= dt
            if self._timer <= 0.0:
                self.knocked = False
                self._hits = 0

        angle = approach(self._angle, 90.0 if self.knocked else 0.0, 540.0 * dt)
        if angle != self._angle:
            self._angle = angle
            base = self._base_rotation
            self._entity.rotation = engine.Vec3(base[0], base[1] + angle, base[2])

        if self._flash > 0.0:
            self._flash = max(0.0, self._flash - dt * 8.0)
            pulse = 1.0 + 0.15 * self._flash
            base = self._base_scale
            self._entity.scale = engine.Vec3(base[0] * pulse, base[1] * pulse, base[2] * pulse)
