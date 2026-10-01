import os
import platform
from pathlib import Path
from typing import Callable

from ..config import GENERATORS

CompileCommandBuilder = Callable[[Path, str, int, str], list[str]]

# Maps a Premake generator name to a function that builds the shell command
# used to compile a project configured with that generator. Register an
# entry here to support a new generator — compile_project() itself stays
# unchanged.
COMPILE_COMMAND_BUILDERS: dict[str, CompileCommandBuilder] = {}


def register(name: str, builder: CompileCommandBuilder) -> None:
    COMPILE_COMMAND_BUILDERS[name] = builder
    GENERATORS.register(name)


# The compilers Premake's gmake toolset defaults to per OS, which a launcher has to be prepended to.
DEFAULT_COMPILERS = {"Darwin": ("clang", "clang++"), "Linux": ("gcc", "g++")}


def _gmake_compile_command(makefile_dir: Path, config_token: str, jobs: int, launcher: str = "") -> list[str]:
    command = ["make", "-C", str(makefile_dir), f"-j{jobs or os.cpu_count() or 1}", f"config={config_token}"]
    if launcher:
        system = platform.system()
        if system not in DEFAULT_COMPILERS:
            raise ValueError(f"[build] launcher is not supported on {system}")
        cc, cxx = DEFAULT_COMPILERS[system]
        command += [f"CC={launcher} {cc}", f"CXX={launcher} {cxx}"]
    return command


def _register_builtins() -> None:
    register("gmake", _gmake_compile_command)


_register_builtins()


def build_compile_command(generator: str, config_token: str, makefile_dir: Path, jobs: int = 0, launcher: str = "") -> list[str]:
    """Build the shell command that compiles the project configured with `generator`."""
    builder = COMPILE_COMMAND_BUILDERS.get(generator)
    if builder is None:
        raise ValueError(f"Unsupported generator: {generator}")
    return builder(makefile_dir, config_token, jobs, launcher)
