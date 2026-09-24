import subprocess

from build_system import workspace
from build_system.setup import stale_objects


def _outputs(tmp_project):
    build = tmp_project / "build"
    return (build / "bin").exists(), (build / "bin-int").exists()


def _make_outputs(tmp_project):
    for name in ("bin", "bin-int"):
        (tmp_project / "build" / name / "Debug-linux-x86_64").mkdir(parents=True, exist_ok=True)


def test_first_configure_without_outputs_records_options(tmp_project):
    assert stale_objects.clear_outputs_if_python_changed(["--no-python"], tmp_project / "build") is False
    assert (tmp_project / "build" / ".python-options").read_text(encoding="utf-8") == "--no-python"


def test_first_configure_with_outputs_wipes_them_silently(tmp_project):
    _make_outputs(tmp_project)
    assert stale_objects.clear_outputs_if_python_changed(["--no-python"], tmp_project / "build") is False
    assert _outputs(tmp_project) == (False, False)


def test_unchanged_options_keep_outputs(tmp_project):
    stale_objects.clear_outputs_if_python_changed(["--no-python", "--sanitize"], tmp_project / "build")
    _make_outputs(tmp_project)
    assert stale_objects.clear_outputs_if_python_changed(["--no-python", "--sanitize"], tmp_project / "build") is False
    assert _outputs(tmp_project) == (True, True)


def test_changed_options_wipe_outputs(tmp_project):
    stale_objects.clear_outputs_if_python_changed(["--no-python"], tmp_project / "build")
    _make_outputs(tmp_project)
    assert stale_objects.clear_outputs_if_python_changed(["--no-python", "--sanitize"], tmp_project / "build") is True
    assert _outputs(tmp_project) == (False, False)
    assert (tmp_project / "build" / ".python-options").read_text(encoding="utf-8") == "--no-python\n--sanitize"


def test_prune_clears_only_projects_with_missing_prerequisites(tmp_project, project, monkeypatch):
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

    ws = workspace.require(project)
    assert stale_objects.prune_stale_object_dirs(ws, "debug", tmp_project / "build") == ["Tests"]
    assert calls == [
        (["make", "-n", "-f", "Oryx.make", "config=debug_x64"], tmp_project / "build"),
        (["make", "-n", "-f", "Tests.make", "config=debug_x64"], tmp_project / "build"),
    ]
    assert (object_root / "Oryx").exists()
    assert not (object_root / "Tests").exists()


def test_prune_without_make_is_a_no_op(tmp_project, project, monkeypatch):
    def missing_make(*args, **kwargs):
        raise FileNotFoundError("make")

    monkeypatch.setattr(stale_objects, "run_command", missing_make)
    assert stale_objects.prune_stale_object_dirs(workspace.require(project), "debug", tmp_project / "build") == []
