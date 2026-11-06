import engine

from platformer_utils import approach, fit_collider


class TriggerZone(engine.Behaviour):
    editor_fields = {
        "target": {"type": "entity", "label": "Target (door)", "default": None},
        "action": {
            "type": "enum", "label": "Action on enter", "default": "open",
            "options": ["none", "open", "close", "toggle", "hold"],
        },
        "once": {"type": "bool", "label": "Only once", "default": False},
        "message": {"type": "str", "label": "HUD message while inside", "default": ""},
        "margin": {"type": "float", "label": "Detection margin", "default": 0.15, "min": 0.0, "max": 3.0},
        "press_depth": {"type": "float", "label": "Press depth (plates)", "default": 0.0, "min": 0.0, "max": 1.0},
    }

    player_inside = False

    def on_start(self):
        self._entity = self.entity
        self._player = engine.find("Player")
        self._rest = self._entity.position
        self._used = False
        self._pressed = 0.0
        self._hud_key = "4_hint_" + str(self._entity.id)
        self.player_inside = False
        self.activations = 0
        fit_collider(self._entity, self.margin)

    def on_trigger_enter(self, other):
        if other != self._player or self.player_inside:
            return

        self.player_inside = True
        self._enter()
        if self.message:
            engine.hud(self._hud_key, self.message)

    def on_trigger_exit(self, other):
        if other != self._player or not self.player_inside:
            return

        self.player_inside = False
        self._exit()
        if self.message:
            engine.hud_clear(self._hud_key)

    def on_field_changed(self, name):
        if name == "margin":
            fit_collider(self._entity, self.margin)

    def on_update(self, dt):
        if self.press_depth <= 0.0:
            return

        pressed = approach(self._pressed, self.press_depth if self.player_inside else 0.0, 0.6 * dt)
        if pressed != self._pressed:
            self._pressed = pressed
            rest = self._rest
            self._entity.position = engine.Vec3(rest.x, rest.y - pressed, rest.z)

    def _target_behaviour(self):
        target = self.target
        if target is None:
            return None
        return target.behaviour

    def _enter(self):
        if self.once and self._used:
            return

        door = self._target_behaviour()
        if self.action != "none" and door is not None:
            if self.action in ("open", "hold"):
                door.open()
            elif self.action == "close":
                door.close()
            elif self.action == "toggle":
                door.toggle()

        self._used = True
        self.activations += 1

    def _exit(self):
        if self.action != "hold":
            return

        door = self._target_behaviour()
        if door is not None:
            door.close()
