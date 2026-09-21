import oryx


class MonteCarlo(oryx.Strategy, id="monte-carlo"):
    """Flat Monte Carlo: scores each legal action by the mean reward of random playouts."""

    playouts: int = 30
    seed: int = 1

    def __init__(self):
        self.random = oryx.Random(self.seed)

    def decide(self, context):
        state = context.state
        player = state.current_player()
        return max(state.legal_actions(), key=lambda action: self.score(state, action, player))

    def score(self, state, action, player):
        state.apply(action)
        total = sum(self.playout(state, player) for _ in range(self.playouts))
        state.undo(action)
        return total

    def playout(self, state, player):
        played = []
        while not state.is_terminal():
            actions = state.legal_actions()
            action = actions[self.random.get_int(0, len(actions) - 1)]
            state.apply(action)
            played.append(action)
        reward = state.outcome()[player]
        for action in reversed(played):
            state.undo(action)
        return reward
