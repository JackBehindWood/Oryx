"""Guess a dropped-in dependency's layout from its files."""

from pathlib import Path

SOURCE_SUFFIXES = (".c", ".cc", ".cpp", ".cxx")
HEADER_SUFFIXES = (".h", ".hh", ".hpp", ".hxx")


def _has(directory: Path, suffixes: tuple[str, ...]) -> bool:
    return directory.is_dir() and any(path.suffix in suffixes for path in directory.rglob("*") if path.is_file())


def detect_layout(directory: Path, name: str) -> dict[str, str]:
    """{kind, include, sources}: `include/` or a same-named header folder is the include dir; `src/` sources make it static."""
    if (directory / "include").is_dir():
        include = "include"
    elif _has(directory / name, HEADER_SUFFIXES):
        include = name
    else:
        include = ""
    if _has(directory / "src", SOURCE_SUFFIXES):
        return {"kind": "static", "include": include, "sources": "src"}
    return {"kind": "header", "include": include, "sources": ""}
