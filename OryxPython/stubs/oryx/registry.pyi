"""
Creating, listing, describing and registering games and strategies by name.
"""
from __future__ import annotations
import oryx.game
import typing
__all__: list[str] = ['describe_game', 'describe_strategy', 'list_games', 'list_strategies', 'make_game', 'make_strategy', 'register_game', 'register_strategy']
def describe_game(name: str) -> dict:
    ...
def describe_strategy(name: str) -> dict:
    ...
def list_games() -> list[str]:
    ...
def list_strategies() -> list[str]:
    ...
def make_game(name: str, **kwargs) -> oryx.game.GameHandle:
    """
    Creates a registered game; keyword arguments are its parameters.
    """
def make_strategy(name: str, **kwargs) -> oryx.game.StrategyHandle:
    """
    Creates a registered strategy; keyword arguments are its parameters.
    """
def register_game(id: str, factory: typing.Any, params: typing.Any = None, description: str = '', overwrite: bool = False) -> None:
    """
    Registers a factory function returning a game; `params` maps names to defaults (or to bool/int/float/str for required ones).
    """
def register_strategy(id: str, factory: typing.Any, params: typing.Any = None, description: str = '', overwrite: bool = False) -> None:
    """
    Registers a factory function returning a strategy.
    """
