import json
import shutil

import pytest

from build_system.config import BuildConfig
from build_system.utils import merge_by_key
from build_system.vscode import c_cpp_properties, launch, settings, tasks

ORYX_TASKS = [
    "Oryx: Configure",
    "Oryx: Compile (Debug)",
    "Oryx: Compile (Release)",
    "Oryx: Compile (Dist)",
    "Oryx: Compile (choose profile)",
    "Oryx: Test",
    "Oryx: Run Oasis",
    "Oryx: All",
    "Oryx: Clean",
]


def _write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data), encoding="utf-8")


def _read(path):
    return json.loads(path.read_text(encoding="utf-8"))


def test_merge_by_key_replaces_generated_and_keeps_rest():
    existing = [{"name": "mine"}, {"name": "gen", "old": True}]
    assert merge_by_key(existing, [{"name": "gen"}], key="name") == [{"name": "mine"}, {"name": "gen"}]


def test_tasks_fresh(tmp_path):
    data = _read(tasks.write_tasks(tmp_path))
    assert data["version"] == "2.0.0"
    assert [task["label"] for task in data["tasks"]] == ORYX_TASKS
    assert data["tasks"][1] == {
        "label": "Oryx: Compile (Debug)",
        "type": "shell",
        "command": "uv run forge --profile debug build compile",
        "group": {"kind": "build", "isDefault": True},
        "problemMatcher": ["$gcc"],
    }
    assert data["tasks"][4]["command"] == "uv run forge --profile ${input:oryxProfile} build compile"


def test_tasks_keep_user_entries(tmp_path):
    path = tmp_path / ".vscode" / "tasks.json"
    user_task = {"label": "Lint", "type": "shell", "command": "ruff check"}
    _write(path, {"version": "2.0.0", "custom": 1, "tasks": [{"label": "Oryx: Configure", "command": "stale"}, user_task]})
    data = _read(tasks.write_tasks(tmp_path))
    assert data["custom"] == 1
    assert data["tasks"][0] == user_task
    assert [task["label"] for task in data["tasks"][1:]] == ORYX_TASKS
    assert data["tasks"][1]["command"] == "uv run forge build configure"


def test_launch_entries_per_profile(tmp_project):
    path = tmp_project / ".vscode" / "launch.json"
    _write(path, {"configurations": [{"name": "Attach", "type": "lldb", "request": "attach"}]})
    data = _read(launch.write_launch(BuildConfig(root=tmp_project), tmp_project, debugger="lldb"))
    bin_dir = tmp_project / "build" / "bin"
    assert data["version"] == "0.2.0"
    assert data["configurations"][0]["name"] == "Attach"
    assert data["configurations"][1:] == [
        {
            "name": f"Oryx: Debug Oasis ({label})",
            "type": "lldb",
            "request": "launch",
            "program": str(bin_dir / outputdir / "Oasis" / "Oasis"),
            "args": [],
            "cwd": "${workspaceFolder}",
            "preLaunchTask": f"Oryx: Compile ({label})",
            "console": "internalConsole",
        }
        for label, outputdir in [("Debug", "Debug-linux-x86_64"), ("Release", "Release-linux-x86_64"), ("Dist", "Dist-linux-x64")]
    ]


@pytest.mark.parametrize(("system", "mode", "tool"), [("Linux", "gdb", "gdb"), ("Darwin", "lldb", "lldb-mi")])
def test_launch_cppdbg_keys(tmp_project, monkeypatch, system, mode, tool):
    monkeypatch.setattr(launch.platform, "system", lambda: system)
    monkeypatch.setattr(shutil, "which", lambda name: f"/bin/{name}")
    data = _read(launch.write_launch(BuildConfig(root=tmp_project), tmp_project, debugger="cppdbg"))
    entry = data["configurations"][0]
    assert (entry["type"], entry["console"], entry["MIMode"], entry["miDebuggerPath"]) == ("cppdbg", "integratedTerminal", mode, f"/bin/{tool}")


def test_settings_merge_keeps_user_keys(tmp_project):
    path = tmp_project / ".vscode" / "settings.json"
    _write(path, {"editor.tabSize": 2, "editor.formatOnSave": False, "search.exclude": {"docs/site": True}, "files.exclude": {"*.pyc": True}})
    data = _read(settings.write_settings(tmp_project))
    assert data["editor.tabSize"] == 2
    assert data["editor.formatOnSave"] is True
    assert data["C_Cpp.default.intelliSenseEngine"] == "Tag Parser"
    assert data["search.exclude"] == {
        "docs/site": True,
        "build": True,
        "bin": True,
        "bin-int": True,
        ".git": True,
        "premake/bin": True,
        "Oryx/vendor/pybind11": True,
        "Oryx/vendor/spdlog": True,
        "Oryx/vendor/yaml-cpp": True,
        "tests/vendor/doctest": True,
    }
    assert data["files.exclude"] == {"*.pyc": True, ".DS_Store": True}


def test_c_cpp_properties_linux(tmp_project, fake_python, monkeypatch):
    monkeypatch.setattr(shutil, "which", lambda name: f"/usr/bin/{name}")
    path = tmp_project / ".vscode" / "c_cpp_properties.json"
    _write(path, {"configurations": [{"name": "Custom"}, {"name": "Linux", "stale": True}]})
    data = _read(c_cpp_properties.write_c_cpp_properties(BuildConfig(), tmp_project))
    assert data["version"] == 4
    assert [c["name"] for c in data["configurations"]] == ["Custom", "Linux"]
    linux = data["configurations"][1]
    assert linux["compilerPath"] == "/usr/bin/g++"
    assert linux["compileCommands"] == "${workspaceFolder}/build/compile_commands.json"
    assert linux["defines"] == ["SPDLOG_COMPILED_LIB", "OX_DEBUG", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING", "OX_ENABLE_PYTHON"]
    assert linux["includePath"] == [
        "${workspaceFolder}/Oryx/src",
        "${workspaceFolder}/Oasis/src",
        "${workspaceFolder}/tests",
        "${workspaceFolder}/Oryx/vendor/pybind11",
        "${workspaceFolder}/Oryx/vendor/spdlog",
        "${workspaceFolder}/Oryx/vendor/yaml-cpp",
        "${workspaceFolder}/tests/vendor/doctest",
        "${workspaceFolder}/Oryx/backends/Python",
        "${workspaceFolder}/build/generated",
        "/py/include/python3.11",
        "/usr/include",
        "/usr/local/include",
    ]


@pytest.mark.parametrize(("profile", "defines"), [("release", ["OX_RELEASE", "OX_ENABLE_PROFILING", "OX_ENABLE_MEMORY_TRACKING"]), ("dist", ["OX_DIST"])])
def test_c_cpp_properties_defines_without_python(tmp_project, profile, defines):
    data = _read(c_cpp_properties.write_c_cpp_properties(BuildConfig(profile=profile, python_enabled=False), tmp_project))
    config = data["configurations"][0]
    assert config["defines"] == ["SPDLOG_COMPILED_LIB", *defines]
    assert "${workspaceFolder}/Oryx/vendor/pybind11" not in config["includePath"]


def test_c_cpp_properties_macos_has_two_configurations(tmp_project, monkeypatch):
    monkeypatch.setattr(c_cpp_properties.platform, "system", lambda: "Darwin")
    monkeypatch.setattr(c_cpp_properties, "get_macos_sdk_path", lambda: "/SDK")
    monkeypatch.setattr(c_cpp_properties, "_macos_compiler_path", lambda: "/usr/bin/clang++")
    data = _read(c_cpp_properties.write_c_cpp_properties(BuildConfig(python_enabled=False), tmp_project))
    assert [(c["name"], c["intelliSenseMode"], c["includePath"][-1]) for c in data["configurations"]] == [
        ("Mac ARM64", "macos-clang-arm64", "/opt/homebrew/include"),
        ("Mac x64", "macos-clang-x64", "/usr/local/include"),
    ]
    assert data["configurations"][0]["macFrameworkPath"] == ["/SDK/System/Library/Frameworks"]


def test_invalid_existing_json_raises(tmp_path):
    path = tmp_path / ".vscode" / "tasks.json"
    path.parent.mkdir()
    path.write_text("{ not json", encoding="utf-8")
    with pytest.raises(ValueError, match="is not valid JSON"):
        tasks.write_tasks(tmp_path)
