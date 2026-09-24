import re
from pathlib import Path

PACKAGE = Path(__file__).resolve().parents[1]
PROJECT_NAMES = re.compile(r"Oryx|Oasis")
ALLOWED: dict[str, int] = {}


def _counts() -> dict[str, int]:
    counts = {}
    for path in sorted(PACKAGE.rglob("*.py")):
        relative = path.relative_to(PACKAGE).as_posix()
        if relative.startswith("tests/") or relative.startswith("oryx/"):
            continue
        found = len(PROJECT_NAMES.findall(path.read_text(encoding="utf-8")))
        if found:
            counts[relative] = found
    return counts


def test_project_names_stay_out_of_the_build_system():
    over = {path: count for path, count in _counts().items() if count > ALLOWED.get(path, 0)}
    assert not over, f"Project names hardcoded in build_system (read them from forge.toml or the Premake export instead): {over}"
