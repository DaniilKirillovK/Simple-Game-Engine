import math
import random

import engine

from platformer_utils import approach


SKIN = 0.02
STEP_HEIGHT = 0.5
GROUND_SNAP = 0.1
MAX_FALL_SPEED = 40.0
JUMP_BUFFER = 0.15
COYOTE_TIME = 0.1
SHAKE_TIME = 0.3
STEP_SMOOTH_SPEED = 4.0


class FpsPlayer(engine.Behaviour):
    editor_fields = {
        "move_speed": {"type": "float", "label": "Move speed", "default": 6.0, "min": 1.0, "max": 20.0},
        "sprint_multiplier": {"type": "float", "label": "Sprint multiplier (Shift)", "default": 1.5, "min": 1.0, "max": 3.0},
        "jump_speed": {"type": "float", "label": "Jump speed", "default": 7.5, "min": 1.0, "max": 20.0},
        "gravity": {"type": "float", "label": "Gravity", "default": 20.0, "min": 1.0, "max": 60.0},
        "mouse_sensitivity": {"type": "float", "label": "Mouse sensitivity", "default": 0.12, "min": 0.01, "max": 1.0},
        "invert_y": {"type": "bool", "label": "Invert mouse Y", "default": False},
        "start_yaw": {"type": "float", "label": "Start yaw (deg)", "default": 0.0, "min": -360.0, "max": 360.0},
        "max_health": {"type": "int", "label": "Max health", "default": 100, "min": 1, "max": 1000},
        "invulnerability": {"type": "float", "label": "Invulnerability after hit (s)", "default": 0.6, "min": 0.0, "max": 5.0},
        "radius": {"type": "float", "label": "Body radius", "default": 0.35, "min": 0.1, "max": 1.5},
        "height": {"type": "float", "label": "Body height", "default": 1.8, "min": 0.5, "max": 4.0},
        "eye_height": {"type": "float", "label": "Eye height", "default": 1.6, "min": 0.3, "max": 4.0},
        "fire_cooldown": {"type": "float", "label": "Fire cooldown (s)", "default": 0.18, "min": 0.02, "max": 5.0},
        "shot_range": {"type": "float", "label": "Shot range", "default": 60.0, "min": 1.0, "max": 500.0},
        "shot_damage": {"type": "int", "label": "Shot damage", "default": 1, "min": 1, "max": 100},
        "impact_prefab": {"type": "str", "label": "Impact prefab", "default": "shot_impact"},
        "damage_flash": {"type": "float", "label": "Damage flash strength", "default": 0.55, "min": 0.0, "max": 1.0},
        "damage_shake": {"type": "float", "label": "Damage camera shake", "default": 0.08, "min": 0.0, "max": 0.5},
        "pass_through_tags": {
            "type": "str", "label": "Tags that rays ignore (comma separated)", "default": "Coin,Impact",
        },
    }

    _step_offset = 0.0

    alive = True
    frozen = False
    health = 0
    grounded = False
    ground = None
    death_reason = ""

    def on_start(self):
        self._entity = self.entity
        self.health = self.max_health
        self.alive = True
        self.frozen = False
        self.grounded = False
        self.ground = None
        self.death_reason = ""

        self._vertical_speed = 0.0
        self._cooldown = 0.0
        self._hurt_timer = 0.0
        self._jump_buffer = 0.0
        self._coyote = 0.0
        self._yaw = self.start_yaw
        self._pitch = 0.0
        self._eye = self.eye_height
        self._shake = 0.0

        self._down = engine.Vec3(0.0, -1.0, 0.0)
        self._up = engine.Vec3(0.0, 1.0, 0.0)
        self._directions = {
            (1, 1.0): engine.Vec3(1.0, 0.0, 0.0),
            (1, -1.0): engine.Vec3(-1.0, 0.0, 0.0),
            (0, 1.0): engine.Vec3(0.0, 0.0, 1.0),
            (0, -1.0): engine.Vec3(0.0, 0.0, -1.0),
        }

        self._update_shape()
        self._read_pass_through()

        self._camera = engine.camera()
        engine.set_free_camera(False)
        engine.lock_cursor(True)
        self._update_camera()

    def _sync_collider(self):
        half_height = self.height * 0.5
        try:
            self._entity.collider_size = engine.Vec3(self.radius, half_height, self.radius)
            self._entity.collider_offset = engine.Vec3(0.0, half_height, 0.0)
        except RuntimeError:
            engine.log_warning("FpsPlayer: add a box Collider to the player so triggers, coins and spikes can detect it")

    def feet_height(self):
        return self._entity.position.y

    def eye_position(self):
        p = self._entity.position
        return engine.Vec3(p.x, p.y + self._eye, p.z)

    def forward(self):
        yaw = math.radians(self._yaw)
        pitch = math.radians(self._pitch)
        return engine.Vec3(
            math.sin(yaw) * math.cos(pitch),
            math.sin(pitch),
            -math.cos(yaw) * math.cos(pitch),
        )

    def take_damage(self, amount, reason="", ignore_invulnerability=False):
        if not self.alive or self.frozen:
            return
        if self._hurt_timer > 0.0 and not ignore_invulnerability:
            return

        self.health = max(0, self.health - amount)
        self._hurt_timer = self.invulnerability

        if self.health == 0:
            self.kill(reason or "You died")
            return

        self._shake = SHAKE_TIME
        if self.damage_flash > 0.0:
            strength = min(1.0, self.damage_flash * (0.6 + amount / max(1, self.max_health)))
            engine.screen_flash(engine.Vec3(0.9, 0.0, 0.0), strength, 0.45)

    def kill(self, reason, tint=None):
        if not self.alive:
            return

        self.alive = False
        self.health = 0
        self.death_reason = reason
        self._shake = SHAKE_TIME * 1.5
        engine.screen_flash(tint if tint is not None else engine.Vec3(0.7, 0.0, 0.0), 0.45, 0.0)

    def _update_shape(self):
        corner = self.radius * 0.7
        self._probes = ((0.0, 0.0), (corner, corner), (-corner, corner), (corner, -corner), (-corner, -corner))
        self._sync_collider()

    def _read_pass_through(self):
        self._pass_through = [tag.strip() for tag in self.pass_through_tags.split(",") if tag.strip()]

    def on_field_changed(self, name):
        if name in ("radius", "height"):
            self._update_shape()
        elif name == "pass_through_tags":
            self._read_pass_through()

    def on_update(self, dt):
        dt = min(dt, 0.05)
        self._cooldown = max(0.0, self._cooldown - dt)
        self._hurt_timer = max(0.0, self._hurt_timer - dt)
        self._shake = max(0.0, self._shake - dt)
        self._step_offset = approach(self._step_offset, 0.0, STEP_SMOOTH_SPEED * dt)

        locked = engine.is_cursor_locked()
        if not locked and engine.is_mouse_pressed(0):
            engine.lock_cursor(True)
            locked = True
            self._cooldown = max(self._cooldown, 0.2)

        if locked:
            self._look()
        self._eye = approach(self._eye, self.eye_height if self.alive else self.eye_height * 0.35, 1.5 * dt)

        if self.alive and not self.frozen:
            self._move(dt)

            if locked and self._cooldown == 0.0 and engine.is_mouse_down(0):
                self._shoot()

        self._update_camera()
        self._update_hud()

    def _look(self):
        dx, dy = engine.mouse_delta()
        if dx == 0.0 and dy == 0.0:
            return

        self._yaw += dx * self.mouse_sensitivity
        self._pitch += (dy if self.invert_y else -dy) * self.mouse_sensitivity
        self._pitch = max(-89.0, min(89.0, self._pitch))

    def _move(self, dt):
        yaw = math.radians(self._yaw)
        forward_x, forward_z = math.sin(yaw), -math.cos(yaw)
        right_x, right_z = math.cos(yaw), math.sin(yaw)

        wish_x = 0.0
        wish_z = 0.0
        if engine.is_key_down(engine.Key.W):
            wish_x += forward_x
            wish_z += forward_z
        if engine.is_key_down(engine.Key.S):
            wish_x -= forward_x
            wish_z -= forward_z
        if engine.is_key_down(engine.Key.D):
            wish_x += right_x
            wish_z += right_z
        if engine.is_key_down(engine.Key.A):
            wish_x -= right_x
            wish_z -= right_z

        length = math.hypot(wish_x, wish_z)
        if length > 0.0:
            speed = self.move_speed
            if engine.is_key_down(engine.Key.LEFT_SHIFT):
                speed *= self.sprint_multiplier
            wish_x *= speed / length
            wish_z *= speed / length

        position = self._entity.position
        x, y, z = position.x, position.y, position.z

        if self.grounded and self.ground is not None:
            carrier = self.ground.behaviour
            delta = getattr(carrier, "delta", None) if carrier is not None else None
            if delta is not None:
                x += delta.x
                y += delta.y
                z += delta.z

        with engine.zone("collisions"):
            x += self._sweep(x, y, z, 1, wish_x * dt)
            z += self._sweep(x, y, z, 0, wish_z * dt)

        if engine.is_key_pressed(engine.Key.SPACE):
            self._jump_buffer = JUMP_BUFFER
        else:
            self._jump_buffer = max(0.0, self._jump_buffer - dt)

        self._coyote = COYOTE_TIME if self.grounded else max(0.0, self._coyote - dt)

        if self._jump_buffer > 0.0 and self._coyote > 0.0:
            self._vertical_speed = self.jump_speed
            self._jump_buffer = 0.0
            self._coyote = 0.0
            self.grounded = False
            self.ground = None

        self._vertical_speed = max(self._vertical_speed - self.gravity * dt, -MAX_FALL_SPEED)
        with engine.zone("ground probe", 0x44AA44) as zone:
            surface = self._vertical(x, y, z, self._vertical_speed * dt)
            if self.grounded and surface > y:
                self._step_offset = max(self._step_offset - (surface - y), -STEP_HEIGHT)
            y = surface
            zone.text("grounded" if self.grounded else "in air")

        self._entity.position = engine.Vec3(x, y, z)

    def _sweep(self, x, y, z, along_x, amount):
        if amount == 0.0:
            return 0.0

        sign = 1.0 if amount > 0.0 else -1.0
        direction = self._directions[(along_x, sign)]
        reach = self.radius + abs(amount) + SKIN
        allowed = abs(amount)

        for height in (0.2, self.height * 0.5, self.height - 0.1):
            for side in (-0.8, 0.0, 0.8):
                offset = side * self.radius
                if along_x:
                    origin = engine.Vec3(x, y + height, z + offset)
                else:
                    origin = engine.Vec3(x + offset, y + height, z)
                hit = engine.raycast(origin, direction, reach, exclude=self._pass_through)
                if hit is not None:
                    allowed = min(allowed, max(0.0, hit.distance - self.radius - SKIN))
                    if allowed == 0.0:
                        return 0.0

        return allowed * sign

    def _vertical(self, x, y, z, dy):
        if dy <= 0.0:
            reach = STEP_HEIGHT - dy + (GROUND_SNAP if self.grounded else 0.0)
            best_distance = None
            best_entity = None

            for offset_x, offset_z in self._probes:
                origin = engine.Vec3(x + offset_x, y + STEP_HEIGHT, z + offset_z)
                hit = engine.raycast(origin, self._down, reach, exclude=self._pass_through)
                if hit is not None:
                    distance = hit.distance
                    if best_distance is None or distance < best_distance:
                        best_distance = distance
                        best_entity = hit.entity

            if best_entity is not None:
                self._vertical_speed = 0.0
                self.grounded = True
                self.ground = best_entity
                return y + STEP_HEIGHT - best_distance

            self.grounded = False
            self.ground = None
            return y + dy

        self.grounded = False
        self.ground = None
        head = self.height - 0.3

        for offset_x, offset_z in self._probes:
            origin = engine.Vec3(x + offset_x, y + head, z + offset_z)
            hit = engine.raycast(origin, self._up, 0.3 + dy, exclude=self._pass_through)
            if hit is not None:
                self._vertical_speed = 0.0
                return y + max(0.0, hit.distance - 0.3)

        return y + dy

    @engine.profile
    def _shoot(self):
        self._cooldown = self.fire_cooldown

        direction = self.forward()
        hit = engine.raycast(self.eye_position(), direction, self.shot_range, exclude=self._pass_through)
        if hit is None:
            return

        target = hit.entity.behaviour
        if target is not None and hasattr(target, "on_shot"):
            target.on_shot(self.shot_damage, hit.point)

        if self.impact_prefab:
            try:
                engine.spawn_prefab(self.impact_prefab, hit.point - direction * 0.05)
            except RuntimeError as error:
                engine.log_warning(str(error))
                self.impact_prefab = ""

    def _update_camera(self):
        camera = self._camera
        if camera is None or not camera.is_valid():
            return

        eye = self.eye_position()
        eye = engine.Vec3(eye.x, eye.y + self._step_offset, eye.z)
        if self._shake > 0.0 and self.damage_shake > 0.0:
            amount = self.damage_shake * self._shake / SHAKE_TIME
            eye = engine.Vec3(
                eye.x + random.uniform(-amount, amount),
                eye.y + random.uniform(-amount, amount),
                eye.z + random.uniform(-amount, amount),
            )

        camera.position = eye
        camera.forward = self.forward()

    def _update_hud(self):
        bar_length = 20
        filled = round(bar_length * self.health / max(1, self.max_health))
        bar = "#" * filled + "-" * (bar_length - filled)
        hurt = "  !" if self._hurt_timer > 0.0 and self.alive else ""
        text = f"Health [{bar}] {self.health}/{self.max_health}{hurt}"
        if text != getattr(self, "_hud_text", None):
            self._hud_text = text
            engine.hud("1_health", text)
