import engine


class ArenaPlayer(engine.Behaviour):
    """Arrows move, F shoots: at the nearest enemy with auto_aim,
    otherwise in the last movement direction.

    WASD, Space and Ctrl stay with the editor camera.
    """

    editor_fields = {
        "speed": {"type": "float", "label": "Speed", "default": 6.0, "min": 0.5, "max": 20.0},
        "max_hp": {"type": "int", "label": "Max HP", "default": 5, "min": 1, "max": 100},
        "fire_cooldown": {"type": "float", "label": "Fire cooldown (s)", "default": 0.25, "min": 0.02, "max": 5.0},
        "auto_aim": {"type": "bool", "label": "Auto aim", "default": True},
        "aim_range": {"type": "float", "label": "Auto aim range", "default": 12.0, "min": 1.0, "max": 50.0},
        "projectile_prefab": {"type": "str", "label": "Projectile prefab", "default": "arena_projectile"},
        "arena_half_size": {"type": "float", "label": "Arena half size", "default": 8.5, "min": 1.0, "max": 50.0},
    }

    # Defaults until on_start: other scripts may ask right after the spawn
    hp = 1
    alive = True

    def on_start(self):
        self.reset()

    def reset(self):
        self.hp = self.max_hp
        self.alive = True
        self._cooldown = 0.0
        self._aim = engine.Vec3(0.0, 0.0, -1.0)

    def take_damage(self, amount):
        if not self.alive:
            return

        self.hp = max(0, self.hp - amount)
        if self.hp == 0:
            self.alive = False
            engine.log("Player died")

    def on_update(self, dt):
        self._cooldown = max(0.0, self._cooldown - dt)
        engine.hud("2_hp", f"HP: {self.hp}/{self.max_hp}")

        if not self.alive:
            return

        self._move(dt)

        if engine.is_key_down(engine.Key.F) and self._cooldown == 0.0:
            self._cooldown = self.fire_cooldown
            self._shoot()

    def _move(self, dt):
        move = engine.Vec3(0.0, 0.0, 0.0)
        if engine.is_key_down(engine.Key.LEFT):
            move.x -= 1.0
        if engine.is_key_down(engine.Key.RIGHT):
            move.x += 1.0
        if engine.is_key_down(engine.Key.UP):
            move.z -= 1.0
        if engine.is_key_down(engine.Key.DOWN):
            move.z += 1.0

        if move.length() == 0.0:
            return

        move = move.normalized()
        self._aim = move

        position = self.entity.position + move * (self.speed * dt)
        limit = self.arena_half_size
        position.x = min(max(position.x, -limit), limit)
        position.z = min(max(position.z, -limit), limit)
        self.entity.position = position

    def _shoot(self):
        position = self.entity.position
        direction = self._aim

        if self.auto_aim:
            enemies = engine.find_in_radius(position, self.aim_range, "Enemy")
            if enemies:
                to_enemy = enemies[0].position - position
                to_enemy.y = 0.0
                if to_enemy.length() > 0.0:
                    direction = to_enemy.normalized()

        shot = engine.spawn_prefab(self.projectile_prefab, position + direction * 0.6)

        projectile = shot.behaviour
        if projectile is not None:
            projectile.launch(direction)
