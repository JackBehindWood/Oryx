import oryx


class NimState(oryx.State):
    def __init__(self, stones, max_take):
        self.stones = stones
        self.max_take = max_take
        self.player = 0

    def legal_actions(self):
        return list(range(1, min(self.max_take, self.stones) + 1))

    def apply(self, action):
        self.stones -= action
        self.player = 1 - self.player

    def undo(self, action):
        self.stones += action
        self.player = 1 - self.player

    def current_player(self):
        return self.player

    def is_terminal(self):
        return self.stones == 0

    def outcome(self):
        rewards = [0.0, 0.0]
        if self.stones == 0:
            rewards = [-1.0, -1.0]
            rewards[1 - self.player] = 1.0
        return rewards

    def action_to_string(self, action):
        return f"take {action} (leaves {self.stones - action})"


class Nim(oryx.Game, id="nim"):
    """Players alternate taking 1..max_take stones from a pile; whoever takes the last stone wins."""

    stones: int = 21
    max_take: int = 3
    num_players = 2

    def new_initial_state(self):
        return NimState(self.stones, self.max_take)
