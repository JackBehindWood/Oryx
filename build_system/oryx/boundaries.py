"""Graphics layering checks: include rules over the source tree, and a symbol check over a graphics-off libOryx.a.

Run as `python build_system/oryx/boundaries.py [--symbols <libOryx.a>]`; exits non-zero on any violation.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

GRAPHICS_MODULES = ("Graphics", "Shaders", "Renderer")
HEADLESS_MODULES = ("Game", "Strategy", "Simulation", "Core", "Text")
RANK = {"Graphics": 0, "Shaders": 1, "Renderer": 2}
GPU_ASSET_CACHE = "GpuAssetCache"
BACKEND_FACTORIES = frozenset({"Graphics/RHI/RHI.cpp", "Core/Window.cpp"})
BACKEND_HEADER = re.compile(r"^(?:Null|Metal|MacOS|Glfw)\w*\.(?:h|hpp)$")
HEADLESS_ASSET_FORBIDDEN = ("Oryx/Core/Window.h", "Oryx/Core/Input.h", "Oryx/Events/WindowEvent.h")

_INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.MULTILINE)
_SOURCE_SUFFIXES = {".h", ".hpp", ".cpp", ".mm"}


def _includes(path: Path) -> list[str]:
    return _INCLUDE.findall(path.read_text(encoding="utf-8", errors="replace"))


def _module_of(include: str) -> str | None:
    parts = include.split("/")
    return parts[1] if len(parts) > 2 and parts[0] == "Oryx" else None


def check_includes(oryx_src: Path) -> list[str]:
    violations = []
    root = oryx_src / "Oryx"
    for path in sorted(root.rglob("*")):
        if path.suffix not in _SOURCE_SUFFIXES:
            continue
        module = path.relative_to(root).parts[0]
        for include in _includes(path):
            target = _module_of(include)
            where = f"{path.relative_to(oryx_src).as_posix()}: includes {include}"
            if include.startswith("backends/") or "/backends/" in include:
                violations.append(f"{where} (public headers must not include backends/)")
            elif BACKEND_HEADER.match(include) and path.relative_to(root).as_posix() not in BACKEND_FACTORIES:
                violations.append(f"{where} (only the backend factories {', '.join(sorted(BACKEND_FACTORIES))} may include a backend header)")
            elif module == "Assets" and GPU_ASSET_CACHE not in path.name and include in HEADLESS_ASSET_FORBIDDEN:
                violations.append(f"{where} (CPU assets must stay headless)")
            elif target in GRAPHICS_MODULES and module in HEADLESS_MODULES:
                violations.append(f"{where} ({module} must not include {target})")
            elif target in GRAPHICS_MODULES and module == "Assets" and GPU_ASSET_CACHE not in path.name:
                violations.append(f"{where} (only {GPU_ASSET_CACHE} may include {target})")
            elif target == "Assets" and (module in GRAPHICS_MODULES or module == "Text"):
                violations.append(f"{where} ({module} must not include Assets; fonts reach the renderer through Text/IFontSource)")
            elif target in RANK and module in RANK and RANK[target] > RANK[module]:
                violations.append(f"{where} ({module} must not include the higher layer {target})")
    return violations


def check_symbols(library: Path) -> list[str]:
    output = subprocess.run(["nm", "-C", str(library)], capture_output=True, text=True, check=True).stdout
    pattern = re.compile(r"\boryx::(?:%s)::" % "|".join(GRAPHICS_MODULES))
    return sorted({line.strip() for line in output.splitlines() if pattern.search(line)})


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--src", type=Path, default=Path(__file__).resolve().parents[2] / "Oryx" / "src")
    parser.add_argument("--symbols", type=Path, metavar="LIB", help="graphics-off libOryx.a to scan with nm")
    args = parser.parse_args(argv)

    problems = check_includes(args.src)
    if args.symbols:
        problems += [f"graphics-off library exports {symbol}" for symbol in check_symbols(args.symbols)]
    for problem in problems:
        print(f"boundary violation: {problem}", file=sys.stderr)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
