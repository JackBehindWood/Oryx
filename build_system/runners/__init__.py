"""Test runners: a suite's directory decides which of these builds its command line."""

from pathlib import Path
from typing import Literal

RunnerKind = Literal["doctest", "pytest"]


def runner_kind(directory: Path) -> RunnerKind:
    """A suite whose directory has a conftest.py or test_*.py runs under pytest; anything else is doctest."""
    if not directory.is_dir():
        return "doctest"
    if (directory / "conftest.py").is_file():
        return "pytest"
    if any(directory.glob("test_*.py")):
        return "pytest"
    return "doctest"


__all__ = ["RunnerKind", "runner_kind"]
