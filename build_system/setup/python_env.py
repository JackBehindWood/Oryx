"""Locate the Python that Oryx's embedded scripting backend builds against.

Read from the interpreter running build_system (the uv venv), so the headers, libpython and
standard library the C++ side links are the ones `uv run` uses. The values reach Premake as
the --python-* options declared in premake/python.lua.
"""

from dataclasses import dataclass
import os
from pathlib import Path
import platform
import sys
import sysconfig

from build_system.config import BuildConfig, PROJECT_ROOT


class PythonEnvError(RuntimeError):
    pass


@dataclass(frozen=True)
class PythonBuildInfo:
    include_dir: Path
    lib_dir: Path
    lib_name: str
    home: Path
    site_packages: tuple[Path, ...]


PACKAGE_DIR = PROJECT_ROOT / "Oryx" / "backends" / "Python"

_NO_LIBPYTHON_HINT = "Use `uv run build --no-python ...`, or a Python built with a shared libpython."


def _windows_library(lib_dir: Path) -> tuple[Path, str]:
    name = f"python{sys.version_info.major}{sys.version_info.minor}"
    return lib_dir / f"{name}.lib", name


def _unix_library(lib_dir: Path) -> tuple[Path, str]:
    file_name = sysconfig.get_config_var("LDLIBRARY") or ""
    stem = file_name.removeprefix("lib").split(".so")[0].removesuffix(".dylib").removesuffix(".a")
    return lib_dir / file_name, stem


def _site_packages() -> tuple[Path, ...]:
    paths = sysconfig.get_paths()
    return tuple(dict.fromkeys(Path(paths[key]) for key in ("purelib", "platlib")))


def python_build_info() -> PythonBuildInfo:
    home = Path(sys.base_prefix)
    include_dir = Path(sysconfig.get_config_var("INCLUDEPY") or sysconfig.get_paths()["include"])

    if platform.system() == "Windows":
        lib_dir = home / "libs"
        library, lib_name = _windows_library(lib_dir)
    else:
        lib_dir = Path(sysconfig.get_config_var("LIBDIR") or home / "lib")
        library, lib_name = _unix_library(lib_dir)

    if not (include_dir / "Python.h").is_file():
        raise PythonEnvError(f"Python.h not found in {include_dir}. {_NO_LIBPYTHON_HINT}")
    if not library.is_file() or library.suffix == ".a":
        raise PythonEnvError(f"No shared libpython at {library}. {_NO_LIBPYTHON_HINT}")

    return PythonBuildInfo(include_dir=include_dir, lib_dir=lib_dir, lib_name=lib_name, home=home, site_packages=_site_packages())


def premake_python_options(cfg: BuildConfig) -> list[str]:
    if not cfg.python_enabled:
        return ["--no-python"]

    info = python_build_info()
    return [
        f"--python-include={info.include_dir.as_posix()}",
        f"--python-libdir={info.lib_dir.as_posix()}",
        f"--python-lib={info.lib_name}",
        f"--python-home={info.home.as_posix()}",
        f"--python-package-dir={PACKAGE_DIR.as_posix()}",
        f"--python-site-packages={os.pathsep.join(path.as_posix() for path in info.site_packages)}",
    ]


def embedding_defines(info: PythonBuildInfo) -> dict[str, str]:
    """The string macros premake/python.lua bakes into the backend, for IDE fallbacks."""
    return {
        "OX_PYTHON_HOME": info.home.as_posix(),
        "OX_PYTHON_PACKAGE_DIR": PACKAGE_DIR.as_posix(),
        "OX_PYTHON_SITE_PACKAGES": os.pathsep.join(path.as_posix() for path in info.site_packages),
    }
