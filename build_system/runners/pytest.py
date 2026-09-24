import sys


def command(dirs: list[str], extra: list[str], *, list_only: bool = False) -> list[str]:
    """`sys.executable -m pytest <dirs>`, so the venv/interpreter forge itself runs under is the one used."""
    flags = ["--collect-only", "-q"] if list_only else []
    return [sys.executable, "-m", "pytest", *dirs, *flags, *extra]
