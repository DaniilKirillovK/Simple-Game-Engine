import engine

SPEED = 90.0


class Rotator(engine.Behaviour):
    def on_start(self):
        engine.log(f"Rotator started on '{self.entity.name}'")

    def on_update(self, dt):
        rotation = self.entity.rotation
        rotation.y += SPEED * dt
        self.entity.rotation = rotation

    def on_destroy(self):
        engine.log(f"Rotator stopped on '{self.entity.name}'")
