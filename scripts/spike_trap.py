import engine

from platformer_utils import approach, fit_collider, player_behaviour


class SpikeTrap(engine.Behaviour):
    editor_fields = {
        "extend_height": {"type": "float", "label": "Extend height", "default": 0.9, "min": 0.1, "max": 5.0},
        "rest_time": {"type": "float", "label": "Hidden time (s)", "default": 1.6, "min": 0.0, "max": 30.0},
        "warn_time": {"type": "float", "label": "Warning time (s)", "default": 0.5, "min": 0.0, "max": 10.0},
        "active_time": {"type": "float", "label": "Extended time (s)", "default": 1.0, "min": 0.1, "max": 30.0},
        "speed": {"type": "float", "label": "Speed (m/s)", "default": 8.0, "min": 0.5, "max": 50.0},
        "damage": {"type": "int", "label": "Damage", "default": 35, "min": 1, "max": 1000},
        "phase_offset": {"type": "float", "label": "Phase offset (s)", "default": 0.0, "min": 0.0, "max": 60.0},
    }

    extended = False

    def on_start(self):
        self._entity = self.entity
        rest = self._entity.position
        self._rest = (rest.x, rest.y, rest.z)
        self._time = self.phase_offset
        self._offset = 0.0
        self._hit_this_cycle = False
        self.extended = False
        self._player_entity = engine.find("Player")
        self._player = player_behaviour(self._player_entity)
        self._player_inside = False
        fit_collider(self._entity, 0.05)

    def on_trigger_enter(self, other):
        if other == self._player_entity:
            self._player_inside = True

    def on_trigger_exit(self, other):
        if other == self._player_entity:
            self._player_inside = False

    def on_update(self, dt):
        cycle = max(0.1, self.rest_time + self.warn_time + self.active_time)
        self._time = (self._time + dt) % cycle

        if self._time < self.rest_time:
            target = 0.0
            self._hit_this_cycle = False
        elif self._time < self.rest_time + self.warn_time:
            target = min(0.15, self.extend_height)
        else:
            target = self.extend_height

        offset = approach(self._offset, target, self.speed * dt)
        if offset != self._offset:
            self._offset = offset
            rest = self._rest
            self._entity.position = engine.Vec3(rest[0], rest[1] + offset, rest[2])

        self.extended = self._offset > self.extend_height * 0.5

        if self.extended and not self._hit_this_cycle and self._player is not None and self._player_inside:
            self._player.take_damage(self.damage, "Impaled on spikes")
            self._hit_this_cycle = True
