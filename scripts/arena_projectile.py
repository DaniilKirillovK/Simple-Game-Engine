import engine


class ArenaProjectile(engine.Behaviour):
    """Flies straight, damages the first enemy it touches."""

    editor_fields = {
        "speed": {"type": "float", "label": "Speed", "default": 14.0, "min": 0.5, "max": 100.0},
        "damage": {"type": "int", "label": "Damage", "default": 1, "min": 0, "max": 100},
        "lifetime": {"type": "float", "label": "Lifetime (s)", "default": 1.5, "min": 0.05, "max": 10.0},
        "hit_radius": {"type": "float", "label": "Hit radius", "default": 0.6, "min": 0.05, "max": 5.0},
    }

    # launch() is called right after the spawn, before on_start
    _direction = engine.Vec3(0.0, 0.0, -1.0)
    _age = 0.0
    _spent = False

    def launch(self, direction):
        self._direction = direction.normalized()

    def on_update(self, dt):
        if self._spent:
            return

        self._age += dt
        if self._age >= self.lifetime:
            self._expire()
            return

        position = self.entity.position + self._direction * (self.speed * dt)
        self.entity.position = position

        hits = engine.find_in_radius(position, self.hit_radius, "Enemy")
        if hits:
            enemy = hits[0].behaviour
            if enemy is not None:
                enemy.take_damage(self.damage)
            self._expire()

    def _expire(self):
        self._spent = True
        self.entity.destroy()
