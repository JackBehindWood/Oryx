"""
Playing matches and batches of matches.
"""
from __future__ import annotations
import collections.abc
import oryx.game
import oryx.results
import typing
__all__: list[str] = ['BatchRunner', 'Match', 'simulate']
class BatchRunner:
    """
    Plays many matches of one game between the same strategies.
    """
    def __init__(self, game: typing.Any, strategies: collections.abc.Sequence) -> None:
        ...
    def run(self, matches: typing.SupportsInt | typing.SupportsIndex) -> oryx.results.BatchResult:
        ...
class Match:
    """
    One game between strategies, stepped by hand or played to the end.
    """
    def __init__(self, game: typing.Any, strategies: collections.abc.Sequence) -> None:
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
    def redo(self) -> typing.Any:
        ...
    def state(self) -> oryx.game.StateHandle:
        ...
    def undo(self) -> typing.Any:
        ...
def simulate(game: typing.Any, strategies: collections.abc.Sequence, games: typing.SupportsInt | typing.SupportsIndex = 1000, seed: typing.Any = None) -> oryx.results.BatchResult:
    """
    Plays `games` matches; strategies created by name that take a `seed` get seed + seat index.
    """
