import math

import engine

from platformer_utils import player_behaviour


class LavaGame(engine.Behaviour):
    editor_fields = {
        "player": {"type": "entity", "label": "Player", "default": None},
        "lava": {"type": "entity", "label": "Lava", "default": None},
        "exit_zone": {"type": "entity", "label": "Exit zone", "default": None},
        "show_help": {"type": "bool", "label": "Show controls", "default": True},
        "auto_restart_delay": {
            "type": "float", "label": "Auto restart after death (s, 0 = only R)",
            "default": 4.0, "min": 0.0, "max": 60.0,
        },
        "intro_time": {"type": "float", "label": "Intro message time (s)", "default": 3.0, "min": 0.0, "max": 30.0},
    }

    state = "playing"
    score = 0

    def on_start(self):
        player = self.player or engine.find("Player")
        lava = self.lava or engine.find("Lava")
        self._player_entity = player
        self._player = player_behaviour(player)
        self._lava = lava.behaviour if lava is not None else None
        self._exit = self.exit_zone or engine.find("Exit")
        self._exit_zone = self._exit.behaviour if self._exit is not None else None
        self.state = "playing"
        self.score = 0
        self.coins = 0
        self.coins_total = len(engine.find_all("Coin"))
        self.elapsed = 0.0
        self._restart_timer = 0.0
        self._shown = {}

        engine.hud_center("")
        if self.show_help:
            engine.hud("9_help", "WASD move  Shift sprint  Space jump  LMB shoot  R restart  F1 console  Esc stop")

        if self._player is None:
            engine.log_error("LavaGame: no Player in the scene")

    def add_score(self, value):
        if self.state == "playing":
            self.score += value

    def add_coin(self, value):
        if self.state == "playing":
            self.coins += 1
            self.score += value

    def _coins_text(self):
        return f"Coins {self.coins}/{self.coins_total}" if self.coins_total else ""

    def on_update(self, dt):
        if engine.is_key_pressed(engine.Key.R):
            engine.restart()
            return

        player = self._player
        lava = self._lava

        if self.state == "lost" and self.auto_restart_delay > 0.0:
            self._restart_timer -= dt
            if self._restart_timer <= 0.0:
                engine.restart()
                return

        if self.state == "playing" and player is not None:
            self.elapsed += dt

            if not player.alive:
                self.state = "lost"
                self._restart_timer = self.auto_restart_delay
                if lava is not None:
                    lava.frozen = True
            elif self._exit_zone is not None and getattr(self._exit_zone, "player_inside", False):
                self.state = "won"
                player.frozen = True
                if lava is not None:
                    lava.frozen = True

        self._update_hud(player, lava)

    def _show(self, key, text):
        if self._shown.get(key) == text:
            return
        self._shown[key] = text

        if key == "center":
            engine.hud_center(text)
        elif text:
            engine.hud(key, text)
        else:
            engine.hud_clear(key)

    def _update_hud(self, player, lava):
        reason = player.death_reason if player is not None and player.death_reason else "You died"

        if self.state == "won":
            self._show("center", f"YOU ESCAPED!\n{self.elapsed:.1f} s    score {self.score}    {self._coins_text()}\nPress R to play again")
            self._show("0_state", f"YOU ESCAPED in {self.elapsed:.1f} s!  Score {self.score}  {self._coins_text()}.  Press R to play again")
        elif self.state == "lost":
            if self.auto_restart_delay > 0.0:
                seconds = max(1, math.ceil(self._restart_timer))
                self._show("center", f"{reason}\nRestarting in {seconds}...  (R - now)")
            else:
                self._show("center", f"{reason}\nPress R to restart")
            self._show("0_state", f"{reason}.  Press R to restart")
        else:
            intro = self.elapsed < self.intro_time
            self._show("center", "ESCAPE THE LAVA\nClimb up to the green exit" if intro else "")
            self._show("0_state", f"Time {self.elapsed:5.1f} s   Score {self.score}   {self._coins_text()}")

        if lava is not None and self.state == "playing" and player is not None:
            waiting = lava.time_until_rise()
            if waiting > 0.0:
                self._show("2_lava", f"Lava starts rising in {waiting:.0f} s")
            else:
                gap = player.feet_height() - lava.surface_height()
                self._show("2_lava", f"Lava is {max(0.0, gap):.1f} m below you")
        else:
            self._show("2_lava", "")

        self._show("8_mouse", "" if engine.is_cursor_locked() else "Click the viewport to capture the mouse")
