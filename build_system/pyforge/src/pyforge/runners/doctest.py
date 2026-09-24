from pathlib import Path


def source_filter(dirs: list[str]) -> str:
    return ",".join(f"*{d}/*" for d in dirs)


def command(binary: Path, dirs: list[str], extra: list[str], *, list_only: bool = False) -> list[str]:
    """The Tests binary, filtered to the given suite directories via --source-file."""
    flags = ["--list-test-cases"] if list_only else []
    return [str(binary), f"--source-file={source_filter(dirs)}", *flags, *extra]
