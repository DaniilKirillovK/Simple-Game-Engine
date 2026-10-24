import random

import engine


class ArenaDirector(engine.Behaviour):
    """Rules of the arena mode: waves, score, victory and defeat, restart.

    States: pause -> spawning -> fighting -> pause ... -> won / lost.
    All numbers come from arena_director.prefab.json (or the inspector).
    """

    editor_fields = {
        "player_prefab": {"type": "str", "label": "Player prefab", "default": "arena_player"},
        "enemy_prefab": {"type": "str", "label": "Enemy prefab", "default": "arena_enemy"},
        "elite_prefab": {"type": "str", "label": "Elite prefab", "default": "arena_enemy_fast"},
        "elite_every": {
            "type": "int", "label": "Every Nth enemy is elite (0 = never)",
            "default": 4, "min": 0, "max": 20,
        },
        "waves": {"type": "int", "label": "Waves to win", "default": 5, "min": 1, "max": 50},
        "first_wave_size": {"type": "int", "label": "First wave size", "default": 3, "min": 1, "max": 100},
        "wave_growth": {"type": "int", "label": "Enemies added per wave", "default": 2, "min": 0, "max": 50},
        "spawn_interval": {
            "type": "float", "label": "Seconds between spawns",
            "default": 0.6, "min": 0.05, "max": 10.0,
        },
        "wave_pause": {"type": "float", "label": "Pause between waves", "default": 3.0, "min": 0.0, "max": 30.0},
        "arena_half_size": {"type": "float", "label": "Arena half size", "default": 8.5, "min": 1.0, "max": 50.0},
        "win_score": {
            "type": "int", "label": "Win at score (0 = clear all waves)",
            "default": 0, "min": 0, "max": 100000,
        },
    }

    def on_start(self):
        self._enemies = []

        self._player = engine.find("Player")
        if self._player is None:
            self._player = engine.spawn_prefab(self.player_prefab, engine.Vec3(0.0, 0.9, 0.0))

        self._restart()

    # ----- rules -----

    def wave_size(self, wave):
        return self.first_wave_size + (wave - 1) * self.wave_growth

    def is_elite(self, index_in_wave):
        return self.elite_every > 0 and index_in_wave % self.elite_every == 0

    def on_enemy_removed(self, enemy, killed, score):
        """Called by enemies: killed by the player or crashed into them."""
        if killed:
            self.score += score

    # ----- state machine -----

    def on_update(self, dt):
        self._enemies = [enemy for enemy in self._enemies if enemy.is_valid()]

        if engine.is_key_pressed(engine.Key.R):
            self._restart()
            return

        if self._state in ("pause", "spawning", "fighting"):
            player = self._player_behaviour()
            if player is None or not player.alive:
                self._finish(won=False)
            elif self.win_score > 0 and self.score >= self.win_score:
                self._finish(won=True)

        if self._state == "pause":
            self._timer -= dt
            if self._timer <= 0.0:
                self._start_wave(self._wave + 1)

        elif self._state == "spawning":
            self._timer -= dt
            if self._timer <= 0.0 and self._to_spawn > 0:
                self._spawned_in_wave += 1
                self._spawn_enemy(self.is_elite(self._spawned_in_wave))
                self._to_spawn -= 1
                self._timer = self.spawn_interval
            if self._to_spawn == 0:
                self._state = "fighting"

        elif self._state == "fighting":
            if not self._enemies:
                if self._wave >= self.waves:
                    self._finish(won=True)
                else:
                    self._state = "pause"
                    self._timer = self.wave_pause

        self._update_hud()

    def _start_wave(self, wave):
        self._wave = wave
        self._to_spawn = self.wave_size(wave)
        self._spawned_in_wave = 0
        self._timer = 0.0
        self._state = "spawning"
        engine.log(f"Wave {wave}/{self.waves}: {self._to_spawn} enemies")

    def _spawn_enemy(self, elite):
        half = self.arena_half_size
        edge = random.uniform(-half, half)
        side = random.randrange(4)
        x, z = [(-half, edge), (half, edge), (edge, -half), (edge, half)][side]

        height = self._player.position.y if self._player.is_valid() else 0.9

        enemy = engine.spawn_prefab(
            self.elite_prefab if elite else self.enemy_prefab,
            engine.Vec3(x, height, z),
            {"target": self._player, "director": self.entity},
        )
        self._enemies.append(enemy)

    def _finish(self, won):
        self._state = "won" if won else "lost"

        for enemy in self._enemies:
            enemy.destroy()
        self._enemies = []

        if won:
            engine.log(f"Victory! Score: {self.score}")
            engine.hud("3_result", f"VICTORY! Score {self.score}. Press R to play again")
        else:
            engine.log(f"Game over on wave {self._wave}. Score: {self.score}")
            engine.hud("3_result", f"GAME OVER on wave {self._wave}. Press R to restart")

    def _restart(self):
        for enemy in self._enemies:
            if enemy.is_valid():
                enemy.destroy()
        self._enemies = []

        self.score = 0
        self._wave = 0
        self._to_spawn = 0
        self._spawned_in_wave = 0
        self._timer = self.wave_pause
        self._state = "pause"

        player = self._player_behaviour()
        if player is not None:
            player.reset()

        engine.hud_clear("3_result")

    # ----- helpers -----

    def _player_behaviour(self):
        if self._player is None or not self._player.is_valid():
            return None
        return self._player.behaviour

    def _update_hud(self):
        status = f"Wave {self._wave}/{self.waves}   Enemies: {len(self._enemies)}   Score: {self.score}"
        if self._state == "pause":
            status += f"   Next wave in {max(self._timer, 0.0):.1f}s"
        engine.hud("1_wave", status)
