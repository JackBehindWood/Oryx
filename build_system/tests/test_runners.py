import sys
from pathlib import Path

from build_system.runners import doctest, pytest as pytest_runner, runner_kind


def test_runner_kind_defaults_to_doctest_for_a_missing_or_plain_directory(tmp_path):
    assert runner_kind(tmp_path / "does-not-exist") == "doctest"
    plain = tmp_path / "unit"
    plain.mkdir()
    (plain / "test_thing.cpp").touch()
    assert runner_kind(plain) == "doctest"


def test_runner_kind_is_pytest_with_a_conftest(tmp_path):
    (tmp_path / "conftest.py").touch()
    assert runner_kind(tmp_path) == "pytest"


def test_runner_kind_is_pytest_with_a_test_module(tmp_path):
    (tmp_path / "test_thing.py").touch()
    assert runner_kind(tmp_path) == "pytest"


def test_doctest_command():
    binary = Path("/build/Tests")
    assert doctest.command(binary, ["tests/unit"], []) == [str(binary), "--source-file=*tests/unit/*"]
    assert doctest.command(binary, ["tests/unit", "tests/integration"], ["--test-case=*Vec3*"]) == [
        str(binary),
        "--source-file=*tests/unit/*,*tests/integration/*",
        "--test-case=*Vec3*",
    ]
    assert doctest.command(binary, ["tests/unit"], [], list_only=True) == [str(binary), "--source-file=*tests/unit/*", "--list-test-cases"]


def test_pytest_command():
    assert pytest_runner.command(["tests/python"], []) == [sys.executable, "-m", "pytest", "tests/python"]
    assert pytest_runner.command(["tests/python"], ["-k", "foo"]) == [sys.executable, "-m", "pytest", "tests/python", "-k", "foo"]
    assert pytest_runner.command(["tests/python"], [], list_only=True) == [sys.executable, "-m", "pytest", "tests/python", "--collect-only", "-q"]
