"""
Console boards: how a game is shown to and played by a human at the terminal.
"""
from __future__ import annotations
import oryx.game
import typing
__all__: list[str] = ['ConsoleBoard', 'PENDING_ACTION', 'UNDO_ACTION', 'read_move']
class ConsoleBoard:
    """
    Base class of console boards defined in Python: `class NimBoard(oryx.ConsoleBoard, game="nim")` registers on import. Methods left out use the generic board.
    """
    shows_moves: typing.ClassVar[bool] = False
    @classmethod
    def __init_subclass__(cls: typing.Any, *, game: str | None = None, overwrite: bool = False, **kwargs) -> None:
        ...
    def on_turn(self, state: oryx.game.StateHandle) -> None:
        """
        Called every update, including on the finished state; `state` is only valid during the call.
        """
    def poll_action(self, state: oryx.game.StateHandle) -> int:
        """
        Returns the human's move, UNDO_ACTION, or PENDING_ACTION while none is chosen; `state` is only valid during the call.
        """
def read_move(state: oryx.game.StateHandle) -> int:
    """
    Lists the legal moves and reads one from stdin; returns PENDING_ACTION once stdin is exhausted.
    """
PENDING_ACTION: int = 4294967293
UNDO_ACTION: int = 4294967294
