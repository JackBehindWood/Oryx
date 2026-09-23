"""
Games, states and strategies, whether implemented in C++ or in a script.
"""
from __future__ import annotations
import typing
__all__: list[str] = ['ActionFeatures', 'Context', 'Game', 'GameHandle', 'State', 'StateHandle', 'Strategy', 'StrategyHandle']
class ActionFeatures:
    """
    Decodes an action into coordinates, for games that provide it; only valid until decide() returns.
    """
    def decode(self, action: typing.SupportsInt | typing.SupportsIndex) -> list[int]:
        ...
class Context:
    """
    What decide() receives; only valid until decide() returns.
    """
    @property
    def action_features(self) -> ActionFeatures:
        ...
    @property
    def state(self) -> StateHandle:
        ...
class Game:
    """
    Base class of games defined in Python: `class Nim(oryx.Game, id="nim")` registers on import.
    """
    @classmethod
    def __init_subclass__(cls: typing.Any, *, id: str | None = None, overwrite: bool = False, **kwargs) -> None:
        ...
    def new_initial_state(self) -> State:
        """
        Returns the state a match starts from.
        """
class GameHandle:
    """
    A game owned by the engine, whether it is implemented in C++ or in a script.
    """
    def __repr__(self) -> str:
        ...
    def name(self) -> str:
        ...
    def new_initial_state(self) -> StateHandle:
        ...
    def num_players(self) -> int:
        ...
class State:
    """
    Optional base class of states: typed method stubs and a default action_to_string().
    """
    def action_to_string(self, action: typing.SupportsInt | typing.SupportsIndex) -> str:
        ...
    def apply(self, action: typing.SupportsInt | typing.SupportsIndex) -> None:
        """
        Plays an action.
        """
    def current_player(self) -> int:
        """
        The player to move.
        """
    def is_terminal(self) -> bool:
        """
        Whether the game is over.
        """
    def legal_actions(self) -> list[int]:
        """
        The actions the current player may take.
        """
    def outcome(self) -> list[float]:
        """
        One reward per player.
        """
    def undo(self, action: typing.SupportsInt | typing.SupportsIndex) -> None:
        """
        Takes back the action that was played last.
        """
class StateHandle:
    """
    A game state driven by the engine; a state lent to a strategy is only valid during decide().
    """
    def action_to_string(self, action: typing.SupportsInt | typing.SupportsIndex) -> str:
        ...
    def apply(self, action: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def current_player(self) -> int:
        ...
    def is_terminal(self) -> bool:
        ...
    def legal_actions(self) -> list[int]:
        ...
    def outcome(self) -> list[float]:
        ...
    def undo(self, action: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
class Strategy:
    """
    Base class of strategies defined in Python: `class Greedy(oryx.Strategy, id="greedy")` registers on import.
    """
    @classmethod
    def __init_subclass__(cls: typing.Any, *, id: str | None = None, overwrite: bool = False, **kwargs) -> None:
        ...
    def decide(self, context: Context) -> int:
        """
        Returns the action to play; `context.state` is only valid during the call.
        """
class StrategyHandle:
    """
    A strategy owned by the engine, whether it is implemented in C++ or in a script.
    """
