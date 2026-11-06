import engine

from platformer_utils import player_behaviour


class RisingLava(engine.Behaviour):
    editor_fields = {
        "start_delay": {"type": "float", "label": "Start delay (s)", "default": 10.0, "min": 0.0, "max": 300.0},
        "rise_speed": {"type": "float", "label": "Rise speed (m/s)", "default": 0.2, "min": 0.0, "max": 10.0},
        "acceleration": {"type": "float", "label": "Acceleration (m/s^2)", "default": 0.004, "min": 0.0, "max": 1.0},
        "max_height": {"type": "float", "label": "Max surface height", "default": 20.0, "min": -10.0, "max": 200.0},
        "instant_kill": {"type": "bool", "label": "Instant kill", "default": True},
        "damage_per_second": {"type": "int", "label": "Damage per second", "default": 60, "min": 1, "max": 1000},
    }

    frozen = False

    def on_start(self):
        self._entity = self.entity
        position = self._entity.position
        self._x, self._y, self._z = position.x, position.y, position.z
        self._half_height = abs(self._entity.scale.y) * 0.5
        self.frozen = False
        self._elapsed = 0.0
        self._speed = self.rise_speed
        self._damage = 0.0
        self._player = player_behaviour(engine.find("Player"))

    def surface_height(self):
        return self._y + self._half_height

    def time_until_rise(self):
        return max(0.0, self.start_delay - self._elapsed)

    def on_field_changed(self, name):
        if name == "rise_speed":
            self._speed = self.rise_speed

    def on_update(self, dt):
        self._elapsed += dt

        if not self.frozen and self._elapsed >= self.start_delay and self.surface_height() < self.max_height:
            self._speed += self.acceleration * dt
            self._y = min(self._y + self._speed * dt, self.max_height - self._half_height)
            self._entity.position = engine.Vec3(self._x, self._y, self._z)

        engine.plot("Lava surface", self.surface_height())
        self._burn(dt)

    def _burn(self, dt):
        player = self._player
        if player is None or not player.alive:
            return

        if player.feet_height() > self.surface_height() + 0.05:
            self._damage = 0.0
            return

        if self.instant_kill:
            player.kill("The lava got you", engine.Vec3(1.0, 0.35, 0.05))
            return

        self._damage += self.damage_per_second * dt
        whole = int(self._damage)
        if whole > 0:
            self._damage -= whole
            player.take_damage(whole, "The lava got you", True)
