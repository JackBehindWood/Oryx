import re
from pathlib import Path

PACKAGE = Path(__file__).resolve().parents[1] / "src" / "pyforge"
PROJECT_NAMES = re.compile(r"Oryx|Oasis")
ALLOWED: dict[str, int] = {}


def _counts() -> dict[str, int]:
    counts = {}
    for pattern in ("*.py", "*.lua"):
        for path in sorted(PACKAGE.rglob(pattern)):
            relative = path.relative_to(PACKAGE).as_posix()
            found = len(PROJECT_NAMES.findall(path.read_text(encoding="utf-8")))
            if found:
                counts[relative] = found
    return counts


def test_project_names_stay_out_of_pyforge():
    over = {path: count for path, count in _counts().items() if count > ALLOWED.get(path, 0)}
    assert not over, f"Project names hardcoded in pyforge (read them from forge.toml or the Premake export instead): {over}"


def test_tests_never_reference_the_oryx_plugin():
    tests_dir = Path(__file__).resolve().parent
    needle = "build_system" + ".oryx"
    offenders = [path.name for path in sorted(tests_dir.rglob("*.py")) if path != Path(__file__).resolve() and needle in path.read_text(encoding="utf-8")]
    assert not offenders, f"pyforge tests must use the dummy plugin, not the Oryx one: {offenders}"
