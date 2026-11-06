import random

import engine

from platformer_utils import player_behaviour


HIDDEN_DEPTH = 500.0


class CrumblingPlatform(engine.Behaviour):
    editor_fields = {
        "delay": {"type": "float", "label": "Time before falling (s)", "default": 0.7, "min": 0.05, "max": 10.0},
        "fall_gravity": {"type": "float", "label": "Fall gravity", "default": 18.0, "min": 1.0, "max": 100.0},
        "respawn_time": {
            "type": "float", "label": "Respawn after (s, 0 = never)",
            "default": 4.0, "min": 0.0, "max": 120.0,
        },
        "shake": {"type": "float", "label": "Shake amplitude", "default": 0.06, "min": 0.0, "max": 0.5},
    }

    state = "idle"

    def on_start(self):
        self._entity = self.entity
        home = self._entity.position
        self._home = (home.x, home.y, home.z)
        self._player = player_behaviour(engine.find("Player"))
        self._timer = 0.0
        self._fall_speed = 0.0
        self._fall_y = self._home[1]
        self.state = "idle"
        self.delta = engine.Vec3(0.0, 0.0, 0.0)

    def _place(self, x, y, z):
        self._entity.position = engine.Vec3(x, y, z)

    def on_update(self, dt):
        hx, hy, hz = self._home

        if self.state == "idle":
            player = self._player
            if player is not None and player.grounded and player.ground == self._entity:
                self.state = "shaking"
                self._timer = self.delay

        elif self.state == "shaking":
            self._timer -= dt
            if self._timer <= 0.0:
                self.state = "falling"
                self._fall_speed = 0.0
                self._fall_y = hy
                self._timer = 3.0
                self._place(hx, hy, hz)
            else:
                jitter = self.shake
                self._place(hx + random.uniform(-jitter, jitter), hy, hz + random.uniform(-jitter, jitter))

        elif self.state == "falling":
            self._fall_speed += self.fall_gravity * dt
            self._fall_y -= self._fall_speed * dt
            self._timer -= dt
            if self._timer <= 0.0:
                self.state = "gone"
                self._timer = self.respawn_time
                self._place(hx, hy - HIDDEN_DEPTH, hz)
            else:
                self._place(hx, self._fall_y, hz)

        elif self.respawn_time > 0.0:
            self._timer -= dt
            if self._timer <= 0.0:
                self.state = "idle"
                self._place(hx, hy, hz)
