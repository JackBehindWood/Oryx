import subprocess

import pytest

from build_system.config import BuildConfig
from build_system.setup import stale_objects


def _touch(root, *paths):
    for path in paths:
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).touch()


def test_manifest_lists_compiled_sources_only(tmp_project):
    _touch(
        tmp_project,
        "Oryx/src/Core/Log.cpp",
        "Oryx/src/Core/Log.h",
        "Oryx/backends/MacOS/Window.mm",
        "Oryx/backends/Python/Glue.c",
        "Oryx/vendor/spdlog/src/spdlog.cpp",
        "OryxPython/src/Module.cpp",
        "Oasis/src/Oasis/Game/TicTacToe.cpp",
        "Oasis/build/Generated.cpp",
        "tests/unit/test_math.cpp",
        "tests/vendor/doctest/doctest/parts/doctest.cpp",
        "tests/python/test_api.py",
        "Other/src/Ignored.cpp",
    )
    assert stale_objects.source_manifest() == [
        "Oasis/src/Oasis/Game/TicTacToe.cpp",
        "Oryx/backends/MacOS/Window.mm",
        "Oryx/backends/Python/Glue.c",
        "Oryx/src/Core/Log.cpp",
        "OryxPython/src/Module.cpp",
        "tests/unit/test_math.cpp",
    ]


def test_sources_changed(tmp_project):
    _touch(tmp_project, "Oryx/src/A.cpp")
    assert stale_objects.sources_changed() is True
    stale_objects.record_source_manifest()
    assert stale_objects.sources_changed() is False
    _touch(tmp_project, "Oryx/src/B.cpp")
    assert stale_objects.sources_changed() is True


def test_no_manifest_clears_nothing(tmp_project):
    assert stale_objects.clear_outputs_of_removed_sources() == []


@pytest.mark.parametrize(
    ("source", "project"),
    [
        ("Oryx/src/Core/Log.cpp", "Oryx"),
        ("Oryx/backends/Python/Glue.cpp", "Oryx"),
        ("OryxPython/src/Module.cpp", "OryxPython"),
        ("Oasis/src/Oasis/Main.cpp", "Oasis"),
        ("tests/unit/test_math.cpp", "Tests"),
    ],
)
def test_removed_source_clears_its_project_binaries(tmp_project, source, project):
    _touch(tmp_project, source, "Oryx/src/Keep.cpp")
    stale_objects.record_source_manifest()
    bin_dir = tmp_project / "build" / "bin"
    for profile in ("Debug-linux-x86_64", "Release-linux-x86_64"):
        for name in ("Oryx", "OryxPython", "Oasis", "Tests"):
            (bin_dir / profile / name).mkdir(parents=True, exist_ok=True)
    (tmp_project / source).unlink()

    assert stale_objects.clear_outputs_of_removed_sources() == [project]
    assert sorted(p.name for p in (bin_dir / "Debug-linux-x86_64").iterdir()) == sorted(
        {"Oryx", "OryxPython", "Oasis", "Tests"} - {project}
    )
    assert not (bin_dir / "Release-linux-x86_64" / project).exists()


def test_removed_oasis_game_source_clears_only_oasis(tmp_project):
    _touch(tmp_project, "Oasis/src/Oasis/Game/TicTacToeGame.cpp")
    stale_objects.record_source_manifest()
    tests_bin = tmp_project / "build" / "bin" / "Debug-linux-x86_64" / "Tests"
    tests_bin.mkdir(parents=True)
    (tmp_project / "Oasis/src/Oasis/Game/TicTacToeGame.cpp").unlink()

    assert stale_objects.clear_outputs_of_removed_sources() == ["Oasis"]
    assert tests_bin.exists()


def _outputs(tmp_project):
    build = tmp_project / "build"
    return (build / "bin").exists(), (build / "bin-int").exists()


def _make_outputs(tmp_project):
    for name in ("bin", "bin-int"):
        (tmp_project / "build" / name / "Debug-linux-x86_64").mkdir(parents=True, exist_ok=True)


def test_first_configure_without_outputs_records_options(tmp_project):
    assert stale_objects.clear_outputs_if_python_changed(["--no-python"]) is False
    assert (tmp_project / "build" / ".python-options").read_text(encoding="utf-8") == "--no-python"


def test_first_configure_with_outputs_wipes_them_silently(tmp_project):
    _make_outputs(tmp_project)
    assert stale_objects.clear_outputs_if_python_changed(["--no-python"]) is False
    assert _outputs(tmp_project) == (False, False)


def test_unchanged_options_keep_outputs(tmp_project):
    stale_objects.clear_outputs_if_python_changed(["--no-python", "--sanitize"])
    _make_outputs(tmp_project)
    assert stale_objects.clear_outputs_if_python_changed(["--no-python", "--sanitize"]) is False
    assert _outputs(tmp_project) == (True, True)


def test_changed_options_wipe_outputs(tmp_project):
    stale_objects.clear_outputs_if_python_changed(["--no-python"])
    _make_outputs(tmp_project)
    assert stale_objects.clear_outputs_if_python_changed(["--no-python", "--sanitize"]) is True
    assert _outputs(tmp_project) == (False, False)
    assert (tmp_project / "build" / ".python-options").read_text(encoding="utf-8") == "--no-python\n--sanitize"


def test_prune_clears_only_projects_with_missing_prerequisites(tmp_project, monkeypatch):
    calls = []

    def fake_run(command, cwd=None, **kwargs):
        calls.append((command, cwd))
        if "Tests.make" in command:
            raise subprocess.CalledProcessError(2, command, stderr="make: *** No rule to make target '../tests/old.cpp'.")
        raise subprocess.CalledProcessError(2, command, stderr="make: *** some other failure")

    monkeypatch.setattr(stale_objects, "run_command", fake_run)
    object_root = tmp_project / "build" / "bin-int" / "Debug-linux-x86_64"
    for name in ("Oryx", "Tests"):
        (object_root / name).mkdir(parents=True)

    assert stale_objects.prune_stale_object_dirs(BuildConfig()) == ["Tests"]
    assert calls == [
        (["make", "-n", "-f", "Oryx.make", "config=debug_x64"], tmp_project / "build"),
        (["make", "-n", "-f", "Tests.make", "config=debug_x64"], tmp_project / "build"),
    ]
    assert (object_root / "Oryx").exists()
    assert not (object_root / "Tests").exists()


def test_prune_without_make_is_a_no_op(tmp_project, monkeypatch):
    def missing_make(*args, **kwargs):
        raise FileNotFoundError("make")

    monkeypatch.setattr(stale_objects, "run_command", missing_make)
    assert stale_objects.prune_stale_object_dirs(BuildConfig()) == []
