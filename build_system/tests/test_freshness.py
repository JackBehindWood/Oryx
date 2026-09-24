import json
import os

import pytest

from build_system import freshness, workspace
from build_system.tests.conftest import workspace_json, write_workspace


def _touch(root, *paths):
    for path in paths:
        (root / path).parent.mkdir(parents=True, exist_ok=True)
        (root / path).touch()


@pytest.fixture
def ws(tmp_project, project):
    for ws_project in workspace.require(project).projects.values():
        for file in ws_project.files:
            file.parent.mkdir(parents=True, exist_ok=True)
            file.touch()
        ws_project.script.parent.mkdir(parents=True, exist_ok=True)
        ws_project.script.touch()
    _touch(tmp_project, "premake5.lua", "premake/common.lua", "premake/forge.lua")
    return workspace.require(project)


def _bump(path):
    stat = path.stat()
    os.utime(path, ns=(stat.st_atime_ns, stat.st_mtime_ns + 1_000_000_000))


def test_no_stamp_is_stale(project):
    assert freshness.stale(project) == "no previous configure"


def test_fresh_after_record(project, ws):
    freshness.record(project, ws)
    assert freshness.stale(project) is None


def test_added_source_file_is_detected_through_its_directory(project, ws, tmp_project):
    freshness.record(project, ws)
    _touch(tmp_project, "Oryx/src/Core/New.cpp")
    assert freshness.stale(project) == "Oryx/src/Core changed"


def test_new_subdirectory_is_detected_through_the_basedir(project, ws, tmp_project):
    freshness.record(project, ws)
    _touch(tmp_project, "tests/integration/test_new.cpp")
    assert freshness.stale(project) == "tests changed"


def test_removed_source_file_is_detected(project, ws, tmp_project):
    freshness.record(project, ws)
    (tmp_project / "Oasis/src/Oasis/Game/TicTacToeGame.cpp").unlink()
    assert freshness.stale(project) == "Oasis/src/Oasis/Game changed"


@pytest.mark.parametrize("name", ["premake5.lua", "Oasis/premake5.lua", "premake/common.lua", "forge.toml"])
def test_edited_script_or_config_is_detected(project, ws, tmp_project, name):
    freshness.record(project, ws)
    _bump(tmp_project / name)
    assert freshness.stale(project) == f"{name} changed"


def test_creating_a_local_config_is_detected(project, ws, tmp_project):
    freshness.record(project, ws)
    (tmp_project / "forge.local.toml").write_text("", encoding="utf-8")
    assert freshness.stale(project) == "forge.local.toml changed"


def test_editing_a_source_file_is_not_a_reconfigure(project, ws, tmp_project):
    freshness.record(project, ws)
    _bump(tmp_project / "Oryx/src/Core/Log.cpp")
    assert freshness.stale(project) is None


def test_options_hash_mismatch(project, ws):
    freshness.record(project, ws, options_hash="a")
    assert freshness.stale(project, options_hash="a") is None
    assert freshness.stale(project, options_hash="b") == "build options changed"


def test_corrupt_stamp_is_stale(project, ws):
    freshness.stamp_file(project).write_text("{", encoding="utf-8")
    assert freshness.stale(project) == "no previous configure"


def test_projects_that_lost_sources_include_every_compiling_project(project, ws, tmp_project):
    previous = freshness.record(project, ws)
    data = json.loads(workspace_json(tmp_project))
    game = f"{tmp_project.as_posix()}/Oasis/src/Oasis/Game/TicTacToeGame.cpp"
    for name in ("Oasis", "Tests"):
        data["projects"][name]["files"].remove(game)
    assert freshness.projects_that_lost_sources(previous, workspace.parse(data)) == ["Oasis", "Tests"]


def test_header_removal_does_not_count(project, ws, tmp_project):
    previous = freshness.record(project, ws)
    data = json.loads(workspace_json(tmp_project))
    data["projects"]["Oryx"]["files"].remove(f"{tmp_project.as_posix()}/Oryx/src/Core/Log.h")
    assert freshness.projects_that_lost_sources(previous, workspace.parse(data)) == []


def test_no_previous_stamp_loses_nothing(ws):
    assert freshness.projects_that_lost_sources(None, ws) == []


def test_removed_project_counts_as_lost(project, ws, tmp_project):
    previous = freshness.record(project, ws)
    data = json.loads(workspace_json(tmp_project))
    del data["projects"]["OryxPython"]
    assert freshness.projects_that_lost_sources(previous, workspace.parse(data)) == ["OryxPython"]


def test_explicitly_listed_source_deleted_from_disk_counts_as_lost(project, ws, tmp_project):
    previous = freshness.record(project, ws)
    game = tmp_project / "Oasis/src/Oasis/Game/TicTacToeGame.cpp"
    game.unlink()
    data = json.loads(workspace_json(tmp_project))
    data["projects"]["Oasis"]["files"].remove(game.as_posix())
    assert freshness.projects_that_lost_sources(previous, workspace.parse(data)) == ["Oasis", "Tests"]
