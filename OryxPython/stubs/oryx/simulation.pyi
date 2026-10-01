"""
Playing matches and batches of matches.
"""
from __future__ import annotations
import oryx.game
import oryx.observability
import oryx.results
import typing
__all__: list[str] = ['BatchRunner', 'Match', 'simulate']
class BatchRunner:
    """
    Plays many matches of one game between the same strategies.
    """
    def __init__(self, game: str | oryx.game.GameHandle | oryx.game.Game | type[oryx.game.Game], strategies: str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy] | collections.abc.Sequence[str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy]]) -> None:
        ...
    def run(self, matches: typing.SupportsInt | typing.SupportsIndex) -> oryx.results.BatchResult:
        ...
class Match:
    """
    One game between strategies, stepped by hand or played to the end.
    """
    def __init__(self, game: str | oryx.game.GameHandle | oryx.game.Game | type[oryx.game.Game], strategies: str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy] | collections.abc.Sequence[str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy]], *, trace: bool = False) -> None:
        ...
    def apply(self, action: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def current_player(self) -> int:
        ...
    def decide(self) -> int:
        ...
    def history(self) -> list[int]:
        ...
    def is_terminal(self) -> bool:
        ...
    def outcome(self) -> list[float]:
        ...
    def play(self) -> list[float]:
        ...
    def redo(self) -> int | None:
        ...
    def state(self) -> oryx.game.StateHandle:
        ...
    def undo(self) -> int | None:
        ...
    @property
    def trace(self) -> list[oryx.observability.Decision]:
        """
        The decisions made so far when created with trace=True, else empty; undo() does not remove them.
        """
def simulate(game: str | oryx.game.GameHandle | oryx.game.Game | type[oryx.game.Game], strategies: str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy] | collections.abc.Sequence[str | oryx.game.StrategyHandle | oryx.game.Strategy | type[oryx.game.Strategy]], games: typing.SupportsInt | typing.SupportsIndex = 1000, seed: int | None = None, *, trace: bool = False) -> oryx.results.BatchResult:
    """
    Plays `games` matches; strategies created by name that take a `seed` get seed + seat index. `trace=True` keeps every decision in `result.trace`, one list per match.
    """
