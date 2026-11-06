import math

import engine

from platformer_utils import fit_collider, player_behaviour


class Coin(engine.Behaviour):
    editor_fields = {
        "value": {"type": "int", "label": "Score", "default": 50, "min": 0, "max": 100000},
        "spin_speed": {"type": "float", "label": "Spin speed (deg/s)", "default": 150.0, "min": 0.0, "max": 1440.0},
        "bob_height": {"type": "float", "label": "Bob height", "default": 0.12, "min": 0.0, "max": 2.0},
        "bob_speed": {"type": "float", "label": "Bob speed", "default": 2.5, "min": 0.0, "max": 20.0},
        "pickup_margin": {"type": "float", "label": "Pickup margin", "default": 0.25, "min": 0.0, "max": 3.0},
        "pickup_effect": {"type": "str", "label": "Pickup effect prefab", "default": "shot_impact"},
    }

    collected = False

    def on_start(self):
        self._entity = self.entity
        base = self._entity.position
        self._x, self._y, self._z = base.x, base.y, base.z
        self._time = (self._x * 0.37 + self._z * 0.71) % (2.0 * math.pi)
        self._angle = 0.0
        self._player_entity = engine.find("Player")
        self._player = player_behaviour(self._player_entity)
        game = engine.find("Game")
        self._game = game.behaviour if game is not None else None
        self.collected = False
        fit_collider(self._entity, self.pickup_margin)

    def on_trigger_enter(self, other):
        player = self._player
        if self.collected or other != self._player_entity or player is None or not player.alive:
            return
        position = self._entity.position
        self._collect(position.y)

    def on_field_changed(self, name):
        if name == "pickup_margin":
            fit_collider(self._entity, self.pickup_margin)

    def on_update(self, dt):
        if self.collected:
            return

        self._time += dt * self.bob_speed
        self._angle = (self._angle + self.spin_speed * dt) % 360.0
        y = self._y + math.sin(self._time) * self.bob_height

        self._entity.rotation = engine.Vec3(0.0, self._angle, 0.0)
        self._entity.position = engine.Vec3(self._x, y, self._z)


    def _collect(self, y):
        self.collected = True

        if self._game is not None and hasattr(self._game, "add_coin"):
            self._game.add_coin(self.value)

        if self.pickup_effect:
            try:
                engine.spawn_prefab(self.pickup_effect, engine.Vec3(self._x, y, self._z))
            except RuntimeError as error:
                engine.log_warning(str(error))

        self._entity.destroy()
