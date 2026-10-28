import engine


class ArenaEnemy(engine.Behaviour):
    """Chases the target; dies from projectiles, crashes into the player.

    target and director are set by the director at spawn time.
    """

    editor_fields = {
        "speed": {"type": "float", "label": "Speed", "default": 2.0, "min": 0.0, "max": 20.0},
        "hp": {"type": "int", "label": "HP", "default": 2, "min": 1, "max": 100},
        "damage": {"type": "int", "label": "Damage to player", "default": 1, "min": 0, "max": 100},
        "touch_radius": {"type": "float", "label": "Touch radius", "default": 0.9, "min": 0.1, "max": 5.0},
        "score": {"type": "int", "label": "Score for kill", "default": 10, "min": 0, "max": 1000},
        "target": {"type": "entity", "label": "Target", "default": None},
        "director": {"type": "entity", "label": "Director", "default": None},
    }

    # A projectile may hit before on_start runs
    _dead = False

    def take_damage(self, amount):
        if self._dead:
            return

        self.hp -= amount
        if self.hp <= 0:
            self._die(killed=True)

    def on_update(self, dt):
        if self._dead:
            return

        target = self.target
        if target is None:
            return

        player = target.behaviour
        if player is None or not player.alive:
            return

        position = self.entity.position
        target_position = target.position
        to_target = engine.Vec3(target_position.x - position.x, 0.0, target_position.z - position.z)
        distance = to_target.length()

        if distance <= self.touch_radius:
            player.take_damage(self.damage)
            self._die(killed=False)
            return

        step = min(self.speed * dt, distance)
        self.entity.position = position + to_target.normalized() * step

    def _die(self, killed):
        self._dead = True

        director = self.director
        if director is not None and director.behaviour is not None:
            director.behaviour.on_enemy_removed(self.entity, killed, self.score)

        self.entity.destroy()
