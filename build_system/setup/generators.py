import os
from pathlib import Path
from typing import Callable

from ..config import GENERATORS

CompileCommandBuilder = Callable[[Path, str, int], list[str]]

# Maps a Premake generator name to a function that builds the shell command
# used to compile a project configured with that generator. Register an
# entry here to support a new generator — compile_project() itself stays
# unchanged.
COMPILE_COMMAND_BUILDERS: dict[str, CompileCommandBuilder] = {}


def register(name: str, builder: CompileCommandBuilder) -> None:
    COMPILE_COMMAND_BUILDERS[name] = builder
    GENERATORS.register(name)


def _gmake_compile_command(makefile_dir: Path, config_token: str, jobs: int) -> list[str]:
    return ["make", "-C", str(makefile_dir), f"-j{jobs or os.cpu_count() or 1}", f"config={config_token}"]


def _register_builtins() -> None:
    register("gmake", _gmake_compile_command)


_register_builtins()


def build_compile_command(generator: str, config_token: str, makefile_dir: Path, jobs: int = 0) -> list[str]:
    """Build the shell command that compiles the project configured with `generator`."""
    builder = COMPILE_COMMAND_BUILDERS.get(generator)
    if builder is None:
        raise ValueError(f"Unsupported generator: {generator}")
    return builder(makefile_dir, config_token, jobs)
