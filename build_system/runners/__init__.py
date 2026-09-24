"""Test runners: a suite's directory decides which of these builds its command line."""

from pathlib import Path
from typing import Literal, Protocol

RunnerKind = Literal["doctest", "pytest"]


class Runner(Protocol):
    def command(self, binary: Path | None, dirs: list[str], extra: list[str], *, list_only: bool = False) -> list[str]: ...


RUNNERS: dict[str, Runner] = {}


def register(name: str, runner: Runner) -> None:
    RUNNERS[name] = runner


def runner_for(kind: str) -> Runner:
    return RUNNERS[kind]


def runner_kind(directory: Path) -> RunnerKind:
    """A suite whose directory has a conftest.py or test_*.py runs under pytest; anything else is doctest."""
    if not directory.is_dir():
        return "doctest"
    if (directory / "conftest.py").is_file():
        return "pytest"
    if any(directory.glob("test_*.py")):
        return "pytest"
    return "doctest"


class _DoctestRunner:
    def command(self, binary: Path | None, dirs: list[str], extra: list[str], *, list_only: bool = False) -> list[str]:
        from . import doctest

        assert binary is not None
        return doctest.command(binary, dirs, extra, list_only=list_only)


class _PytestRunner:
    def command(self, binary: Path | None, dirs: list[str], extra: list[str], *, list_only: bool = False) -> list[str]:
        from . import pytest

        return pytest.command(dirs, extra, list_only=list_only)


def _register_builtins() -> None:
    register("doctest", _DoctestRunner())
    register("pytest", _PytestRunner())


_register_builtins()


__all__ = ["Runner", "RunnerKind", "register", "runner_for", "runner_kind"]
