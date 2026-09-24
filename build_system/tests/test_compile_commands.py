import json
import platform
import shutil
import subprocess

import pytest

from build_system import compile_commands
from build_system.config import BuildConfig

MAKE_DRY_RUN = """\
==== Building Tests (debug_x64) ====
mkdir -p bin-int/Debug-linux-x86_64/Tests
clang++ -x c++-header -MD -MP -DOX_DEBUG -I../tests -std=c++20 -o "bin-int/Debug-linux-x86_64/Tests/oxpch.h.gch" -MF "bin-int/Debug-linux-x86_64/Tests/oxpch.h.d" -c "../Oryx/src/oxpch.h"
clang++ -MD -MP -DOX_DEBUG -DOX_BUILD_OUTPUT_DIR=\\"build/bin/Debug-linux-x86_64\\" -I../tests -std=c++20 -include bin-int/Debug-linux-x86_64/Tests/oxpch.h -o "bin-int/Debug-linux-x86_64/Tests/test_math.o" -MF "bin-int/Debug-linux-x86_64/Tests/test_math.d" -c "../tests/unit/test_math.cpp"
echo "unbalanced -c quote
ar -rcs bin/Debug-linux-x86_64/Tests/libTests.a
"""


@pytest.fixture
def make_calls(tmp_project, monkeypatch):
    calls = []

    def fake_run(command, cwd=None, **kwargs):
        calls.append((command, cwd))
        return subprocess.CompletedProcess(command, 0, stdout=MAKE_DRY_RUN, stderr="")

    monkeypatch.setattr(compile_commands, "run_command", fake_run)
    monkeypatch.setattr(compile_commands, "get_macos_sdk_path", lambda: "/SDK")
    return calls


def _expected(tmp_project, sdk: list[str]):
    directory = str(tmp_project / "build")
    return [
        {
            "directory": directory,
            "file": "../Oryx/src/oxpch.h",
            "arguments": ["clang++", *sdk, "-x", "c++-header", "-MD", "-MP", "-DOX_DEBUG", "-I../tests", "-std=c++20",
                          "-o", "bin-int/Debug-linux-x86_64/Tests/oxpch.h.gch", "-MF", "bin-int/Debug-linux-x86_64/Tests/oxpch.h.d",
                          "-c", "../Oryx/src/oxpch.h"],
            "output": "bin-int/Debug-linux-x86_64/Tests/oxpch.h.gch",
        },
        {
            "directory": directory,
            "file": "../tests/unit/test_math.cpp",
            "arguments": ["clang++", *sdk, "-MD", "-MP", "-DOX_DEBUG", '-DOX_BUILD_OUTPUT_DIR="build/bin/Debug-linux-x86_64"', "-I../tests",
                          "-std=c++20", "-include", "bin-int/Debug-linux-x86_64/Tests/oxpch.h",
                          "-o", "bin-int/Debug-linux-x86_64/Tests/test_math.o", "-MF", "bin-int/Debug-linux-x86_64/Tests/test_math.d",
                          "-c", "../tests/unit/test_math.cpp"],
            "output": "bin-int/Debug-linux-x86_64/Tests/test_math.o",
        },
    ]


def test_entries_from_make_dry_run(tmp_project, make_calls):
    entries = compile_commands._compile_entries(tmp_project / "build" / "Tests.make", "debug_x64")
    assert make_calls == [(["make", "-n", "-B", "-k", "-f", "Tests.make", "config=debug_x64"], tmp_project / "build")]
    assert entries == _expected(tmp_project, [])


def test_macos_entries_get_explicit_sysroot(tmp_project, make_calls, monkeypatch):
    monkeypatch.setattr(platform, "system", lambda: "Darwin")
    entries = compile_commands._compile_entries(tmp_project / "build" / "Tests.make", "debug_x64")
    assert entries == _expected(tmp_project, ["-isysroot", "/SDK"])


def test_failed_dry_run_still_uses_printed_compile_lines(tmp_project, monkeypatch):
    def failing(command, **kwargs):
        raise subprocess.CalledProcessError(2, command, output=MAKE_DRY_RUN, stderr="missing lib")

    monkeypatch.setattr(compile_commands, "run_command", failing)
    assert len(compile_commands._compile_entries(tmp_project / "build" / "Tests.make", "debug_x64")) == 2


def test_failed_dry_run_without_compile_lines_is_skipped(tmp_project, monkeypatch):
    def failing(command, **kwargs):
        raise subprocess.CalledProcessError(2, command, output="make: *** nothing\n", stderr="bad makefile")

    monkeypatch.setattr(compile_commands, "run_command", failing)
    assert compile_commands._compile_entries(tmp_project / "build" / "Tests.make", "debug_x64") == []


def test_generate_without_make(tmp_project, monkeypatch):
    monkeypatch.setattr(shutil, "which", lambda name: None)
    assert compile_commands.generate_compile_commands(BuildConfig()) is None


def test_generate_without_make_files(tmp_project, monkeypatch):
    monkeypatch.setattr(shutil, "which", lambda name: "/usr/bin/make")
    for make_file in (tmp_project / "build").glob("*.make"):
        make_file.unlink()
    assert compile_commands.generate_compile_commands(BuildConfig()) is None


def test_generate_writes_every_project(tmp_project, make_calls, monkeypatch):
    monkeypatch.setattr(shutil, "which", lambda name: "/usr/bin/make")
    path = compile_commands.generate_compile_commands(BuildConfig(profile="release"))
    assert path == tmp_project / "build" / "compile_commands.json"
    assert [call[0][5:] for call in make_calls] == [["Oryx.make", "config=release_x64"], ["Tests.make", "config=release_x64"]]
    assert json.loads(path.read_text(encoding="utf-8")) == _expected(tmp_project, []) * 2
