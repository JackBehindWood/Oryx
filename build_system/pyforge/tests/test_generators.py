import os
from pathlib import Path

import pytest

from pyforge.setup.generators import build_compile_command


def test_gmake_builds_in_parallel_on_all_cores_by_default():
    command = build_compile_command("gmake", "debug_x64", Path("build"))
    assert command == ["make", "-C", "build", f"-j{os.cpu_count()}", "config=debug_x64"]


def test_gmake_honours_explicit_jobs():
    assert "-j3" in build_compile_command("gmake", "debug_x64", Path("build"), jobs=3)


def test_unknown_generator():
    with pytest.raises(ValueError, match="Unsupported generator: ninja"):
        build_compile_command("ninja", "debug_x64", Path("build"))


@pytest.mark.parametrize("system, cc, cxx", [("Darwin", "clang", "clang++"), ("Linux", "gcc", "g++")])
def test_gmake_launcher_prefixes_the_default_compilers(monkeypatch, system, cc, cxx):
    monkeypatch.setattr("platform.system", lambda: system)
    command = build_compile_command("gmake", "debug_x64", Path("build"), launcher="ccache")
    assert command[-2:] == [f"CC=ccache {cc}", f"CXX=ccache {cxx}"]


def test_gmake_without_launcher_has_no_compiler_overrides():
    assert not any(arg.startswith(("CC=", "CXX=")) for arg in build_compile_command("gmake", "debug_x64", Path("build")))


def test_gmake_launcher_on_an_unsupported_os(monkeypatch):
    monkeypatch.setattr("platform.system", lambda: "Windows")
    with pytest.raises(ValueError, match="launcher is not supported on Windows"):
        build_compile_command("gmake", "debug_x64", Path("build"), launcher="ccache")
