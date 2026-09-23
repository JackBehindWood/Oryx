"""
Creating, listing, describing and registering games and strategies by name.
"""
from __future__ import annotations
import collections.abc
import oryx.game
import typing
__all__: list[str] = ['describe_game', 'describe_strategy', 'list_games', 'list_strategies', 'make_game', 'make_strategy', 'register_game', 'register_strategy']
def describe_game(name: str | type[oryx.game.Game]) -> dict[str, typing.Any]:
    ...
def describe_strategy(name: str | type[oryx.game.Strategy]) -> dict[str, typing.Any]:
    ...
def list_games() -> list[str]:
    ...
def list_strategies() -> list[str]:
    ...
def make_game(name: str | type[oryx.game.Game], **kwargs) -> oryx.game.GameHandle:
    """
    Creates a registered game; keyword arguments are its parameters.
    """
def make_strategy(name: str | type[oryx.game.Strategy], **kwargs) -> oryx.game.StrategyHandle:
    """
    Creates a registered strategy; keyword arguments are its parameters.
    """
def register_game(id: str, factory: collections.abc.Callable[..., typing.Any], params: dict[str, typing.Any] | None = None, description: str = '', overwrite: bool = False) -> None:
    """
    Registers a factory function returning a game; `params` maps names to defaults (or to bool/int/float/str for required ones).
    """
def register_strategy(id: str, factory: collections.abc.Callable[..., typing.Any], params: dict[str, typing.Any] | None = None, description: str = '', overwrite: bool = False) -> None:
    """
    Registers a factory function returning a strategy.
    """
