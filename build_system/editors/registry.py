from typing import Callable

import typer

from ..config import EDITORS

EditorCommand = Callable[[typer.Context, bool], None]
EDITOR_COMMANDS: dict[str, EditorCommand] = {}


def register(name: str, write: EditorCommand) -> None:
    EDITOR_COMMANDS[name] = write
    EDITORS.register(name)


def editor_command(name: str) -> EditorCommand:
    return EDITOR_COMMANDS[name]
