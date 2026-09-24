import json
import platform
from pathlib import Path

import pytest

from build_system import workspace
from build_system.tests.conftest import workspace_json, write_workspace
from build_system.workspace import WorkspaceError, WsConfig, select_config


@pytest.fixture
def ws(tmp_project, project):
    write_workspace(tmp_project)
    return workspace.require(project)


def test_load_before_configure_is_none(project):
    assert workspace.load(project) is None
    with pytest.raises(WorkspaceError, match="run `forge build configure` first"):
        workspace.require(project)


def test_projects_and_scripts(ws, tmp_project):
    assert sorted(ws.projects) == ["Oasis", "Oryx", "OryxPython", "Tests", "spdlog"]
    assert ws.location == tmp_project / "build"
    assert ws.scripts[0] == tmp_project / "premake5.lua"
    assert ws.project("Oasis").script == tmp_project / "Oasis" / "premake5.lua"


def test_target_paths_come_from_the_export(ws, tmp_project):
    bin_dir = tmp_project / "build" / "bin"
    assert ws.target_path("Oasis", "debug") == bin_dir / "Debug-linux-x86_64" / "Oasis" / "Oasis"
    assert ws.target_path("OryxPython", "release") == bin_dir / "Release-linux-x86_64" / "OryxPython" / "oryx.so"
    assert ws.config("Tests", "dist").objdir == tmp_project / "build" / "bin-int" / "Dist-linux-x86_64" / "Tests"


def test_token(ws):
    assert [ws.token(profile) for profile in ("debug", "release", "dist")] == ["debug_x64", "release_x64", "dist_x64"]


def test_config_carries_defines_and_includes(ws, tmp_project):
    cfg = ws.config("Tests", "debug")
    assert cfg.kind == "ConsoleApp"
    assert cfg.defines[-1] == 'OX_BUILD_OUTPUT_DIR="build/bin/Debug-linux-x86_64"'
    assert cfg.includedirs[0] == tmp_project / "tests" / "vendor" / "doctest" / "doctest"


def test_sources_skip_headers(ws, tmp_project):
    assert tmp_project / "Oryx/src/Core/Log.h" not in ws.project("Oryx").sources
    assert len(ws.project("Oryx").sources) == 3


def test_unknown_project_lists_available(ws):
    with pytest.raises(WorkspaceError, match="'Oasys' not found in the workspace. Available: Oasis, Oryx"):
        ws.project("Oasys")


def test_unknown_profile(ws):
    with pytest.raises(WorkspaceError, match="No 'fast' configuration .* Available: Debug, Dist, Release"):
        ws.config("Oasis", "fast")


def test_source_dirs_climb_to_the_basedir_only(ws, tmp_project):
    dirs = ws.source_dirs()
    assert {tmp_project / "Oryx", tmp_project / "Oryx/src", tmp_project / "Oryx/src/Core", tmp_project / "Oryx/backends"} <= dirs
    assert tmp_project / "Oasis/src/Oasis/Game" in dirs
    assert tmp_project not in dirs
    assert tmp_project / "Oryx/vendor/spdlog/src" in dirs
    assert tmp_project / "Oryx/vendor" not in dirs


def _cfg(architecture: str, buildcfg: str = "Debug") -> WsConfig:
    path = Path("/x")
    return WsConfig(buildcfg, architecture, architecture, "ConsoleApp", f"{buildcfg.lower()}_{architecture.lower()}", path, path, path, (), ())


@pytest.mark.parametrize(("machine", "expected"), [("arm64", "AARCH64"), ("aarch64", "AARCH64"), ("x86_64", "x86_64"), ("AMD64", "x86_64")])
def test_config_matches_host_architecture(monkeypatch, machine, expected):
    monkeypatch.setattr(platform, "machine", lambda: machine)
    configs = (_cfg("AARCH64"), _cfg("x86_64"), _cfg("AARCH64", "Release"))
    assert select_config(configs, "debug").architecture == expected


def test_config_falls_back_to_the_first_platform(monkeypatch):
    monkeypatch.setattr(platform, "machine", lambda: "arm64")
    assert select_config((_cfg("x86_64"),), "debug").architecture == "x86_64"


def test_empty_lua_tables_decode_as_empty_lists(tmp_path):
    data = json.loads(workspace_json(tmp_path))
    data["projects"]["Oasis"]["configs"][0]["defines"] = {}
    data["projects"]["Oasis"]["files"] = {}
    parsed = workspace.parse(data)
    assert parsed.project("Oasis").configs[0].defines == ()
    assert parsed.project("Oasis").files == ()


def test_unknown_format_asks_for_reconfigure(tmp_path):
    data = json.loads(workspace_json(tmp_path))
    data["format"] = 99
    with pytest.raises(WorkspaceError, match="format 99"):
        workspace.parse(data)
