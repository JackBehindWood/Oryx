from collections.abc import Callable
from typing import Any

from .game import Game, GameHandle, Strategy, StrategyHandle

ParamValue = bool | int | float | str

def make_game(name: str, **params: ParamValue) -> GameHandle: ...
def make_strategy(name: str, **params: ParamValue) -> StrategyHandle: ...
def list_games() -> list[str]: ...
def list_strategies() -> list[str]: ...
def describe_game(name: str) -> dict[str, Any]: ...
def describe_strategy(name: str) -> dict[str, Any]: ...
def register_game(
    id: str,
    factory: Callable[..., Game],
    params: dict[str, ParamValue | type[ParamValue]] | None = None,
    description: str = "",
    overwrite: bool = False,
) -> None: ...
def register_strategy(
    id: str,
    factory: Callable[..., Strategy],
    params: dict[str, ParamValue | type[ParamValue]] | None = None,
    description: str = "",
    overwrite: bool = False,
) -> None: ...
