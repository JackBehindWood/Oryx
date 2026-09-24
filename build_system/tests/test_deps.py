import subprocess
import tomllib

import pytest

from build_system.config import FetchMode
from build_system.deps import resolve
from build_system.deps.detect import detect_layout
from build_system.deps.resolve import DependencyError, ensure, missing, required, requirements_met
from build_system.deps.sources import submodule as submodule_source
from build_system.tests.conftest import make_run


def _populate(root, *dirs):
    for name in dirs:
        (root / name).mkdir(parents=True, exist_ok=True)
        (root / name / "README").touch()


@pytest.fixture
def git_calls(monkeypatch):
    calls = []

    def fake_run(command, cwd=None, **kwargs):
        calls.append((command, cwd))
        if command[:2] == ["git", "submodule"] and "update" in command:
            path = command[-1]
            (cwd / path).mkdir(parents=True, exist_ok=True)
            (cwd / path / "README").touch()
        return subprocess.CompletedProcess(command, 0, stdout="abc1234\n", stderr="")

    monkeypatch.setattr(submodule_source, "run_command", fake_run)
    return calls


def test_requirements():
    from build_system.config import Dependency

    spec = Dependency(requires=["python", "!sanitize"])
    assert requirements_met(spec.requires, {"python": True, "sanitize": False})
    assert not requirements_met(spec.requires, {"python": False, "sanitize": False})
    assert not requirements_met(spec.requires, {"python": True, "sanitize": True})


def test_required_skips_pybind11_without_python(project):
    names = lambda run: [dep.name for dep in required(run)]
    assert names(make_run(project)) == ["spdlog", "yaml-cpp", "pybind11", "doctest"]
    assert names(make_run(project, python=False)) == ["spdlog", "yaml-cpp", "doctest"]


def test_paths(project, tmp_project):
    deps = {dep.name: dep for dep in required(make_run(project))}
    assert deps["spdlog"].dir == tmp_project / "Oryx/vendor/spdlog"
    assert deps["spdlog"].include == tmp_project / "Oryx/vendor/spdlog/include"
    assert deps["spdlog"].sources == tmp_project / "Oryx/vendor/spdlog/src"
    assert deps["doctest"].include == tmp_project / "tests/vendor/doctest/doctest"
    assert deps["doctest"].sources is None


def test_missing_are_the_empty_or_absent_ones(project, tmp_project):
    _populate(tmp_project, "Oryx/vendor/spdlog", "tests/vendor/doctest")
    (tmp_project / "Oryx/vendor/yaml-cpp").rmdir()
    assert [dep.name for dep in missing(make_run(project))] == ["yaml-cpp", "pybind11"]


def test_premake_config_lists_only_required(project, tmp_project):
    config = resolve.premake_config(make_run(project, python=False))
    assert config["options"] == {"python": False, "sanitize": False}
    assert list(config["dependencies"]) == ["spdlog", "yaml-cpp", "doctest"]
    assert config["dependencies"]["spdlog"] == {
        "kind": "static",
        "dir": f"{tmp_project.as_posix()}/Oryx/vendor/spdlog",
        "include": f"{tmp_project.as_posix()}/Oryx/vendor/spdlog/include",
        "defines": ["SPDLOG_COMPILED_LIB"],
        "sources": f"{tmp_project.as_posix()}/Oryx/vendor/spdlog/src",
    }
    assert "sources" not in config["dependencies"]["doctest"]


def test_write_premake_config_only_when_changed(project):
    run = make_run(project)
    path = resolve.write_premake_config(run)
    stamp = path.stat().st_mtime_ns
    resolve.write_premake_config(run)
    assert path.stat().st_mtime_ns == stamp


def _with_fetch(run, mode):
    from dataclasses import replace

    run.config = replace(run.config, build=replace(run.config.build, fetch=mode))
    return run


def test_fetch_never_fails_listing_what_is_missing(project):
    with pytest.raises(DependencyError, match=r"Missing dependencies: spdlog \(Oryx/vendor/spdlog\), .* Run: forge deps sync"):
        ensure(_with_fetch(make_run(project), FetchMode.NEVER))


def test_fetch_auto_inits_only_missing_required_submodules(project, tmp_project, git_calls):
    _populate(tmp_project, "Oryx/vendor/spdlog", "tests/vendor/doctest")
    fetched = ensure(_with_fetch(make_run(project, python=False), FetchMode.AUTO))
    assert [dep.name for dep in fetched] == ["yaml-cpp"]
    assert git_calls == [(["git", "submodule", "update", "--init", "--recursive", "--", "Oryx/vendor/yaml-cpp"], tmp_project)]


@pytest.mark.parametrize(("answer", "fetched"), [(True, True), (False, False)])
def test_fetch_ask(project, tmp_project, git_calls, answer, fetched):
    _populate(tmp_project, "Oryx/vendor/spdlog", "Oryx/vendor/pybind11", "tests/vendor/doctest")
    run = _with_fetch(make_run(project), FetchMode.ASK)
    if fetched:
        assert [dep.name for dep in ensure(run, confirm=lambda deps: answer)] == ["yaml-cpp"]
    else:
        with pytest.raises(DependencyError, match='set \\[build\\] fetch = "auto"'):
            ensure(run, confirm=lambda deps: answer)


def test_present_dependencies_cost_no_git_call(project, tmp_project, git_calls):
    _populate(tmp_project, "Oryx/vendor/spdlog", "Oryx/vendor/yaml-cpp", "Oryx/vendor/pybind11", "tests/vendor/doctest")
    assert ensure(_with_fetch(make_run(project), FetchMode.AUTO)) == []
    assert git_calls == []


def test_missing_local_dependency_says_where_to_put_it(project, tmp_project, git_calls):
    config = tmp_project / "forge.toml"
    config.write_text(config.read_text(encoding="utf-8").replace('[docs]', 'glad = { source = "local", kind = "static" }\n\n[docs]'), encoding="utf-8")
    with pytest.raises(DependencyError, match="'glad' is a local dependency: put its files at Oryx/vendor/glad"):
        ensure(_with_fetch(make_run(project), FetchMode.AUTO))
    assert git_calls == []


@pytest.mark.parametrize(
    ("files", "layout"),
    [
        (["include/glad/gl.h", "src/gl.c"], {"kind": "static", "include": "include", "sources": "src"}),
        (["include/stb.h"], {"kind": "header", "include": "include", "sources": ""}),
        (["other/x.h"], {"kind": "header", "include": "", "sources": ""}),
        (["lib/lib.hpp", "README"], {"kind": "header", "include": "lib", "sources": ""}),
        (["single.h"], {"kind": "header", "include": "", "sources": ""}),
        (["src/impl.cpp"], {"kind": "static", "include": "", "sources": "src"}),
    ],
)
def test_detect_layout(tmp_path, files, layout):
    for file in files:
        (tmp_path / file).parent.mkdir(parents=True, exist_ok=True)
        (tmp_path / file).touch()
    assert detect_layout(tmp_path, "lib") == layout


def _config(tmp_project):
    return tomllib.loads((tmp_project / "forge.toml").read_text(encoding="utf-8"))


def test_add_local_detects_the_layout_and_keeps_the_rest_of_forge_toml(forge, tmp_project):
    before = (tmp_project / "forge.toml").read_text(encoding="utf-8")
    for file in ("include/glad/gl.h", "src/gl.c"):
        (tmp_project / "Oryx/vendor/glad" / file).parent.mkdir(parents=True, exist_ok=True)
        (tmp_project / "Oryx/vendor/glad" / file).touch()
    result = forge("deps", "add", "glad", "--local")
    assert result.exit_code == 0, result.output
    assert 'glad = { source = "local", kind = "static", include = "include", sources = "src" }' in result.output
    assert 'links { "glad" }' in result.output
    after = (tmp_project / "forge.toml").read_text(encoding="utf-8")
    assert after.replace('glad = { source = "local", kind = "static", include = "include", sources = "src" }\n', "") == before


def test_add_submodule_runs_git_and_honours_overrides(forge, tmp_project, git_calls):
    result = forge("deps", "add", "stb", "--submodule", "https://example.com/stb.git", "--path", "third_party/stb", "--include", ".", "--requires", "python")
    assert result.exit_code == 0, result.output
    assert git_calls[0] == (["git", "submodule", "add", "https://example.com/stb.git", "third_party/stb"], tmp_project)
    assert _config(tmp_project)["dependencies"]["stb"] == {"path": "third_party/stb", "include": ".", "requires": ["python"]}


@pytest.mark.parametrize(
    ("args", "message"),
    [
        (["glad"], "Pass exactly one of --local or --submodule URL."),
        (["glad", "--local", "--submodule", "u"], "Pass exactly one of --local or --submodule URL."),
        (["spdlog", "--local"], "'spdlog' is already in forge.toml [dependencies]."),
        (["glad", "--local"], "Put the files for 'glad' at Oryx/vendor/glad first"),
        (["glad", "--local", "--path", "x", "--requires", "gui"], "names unknown option 'gui'"),
    ],
)
def test_add_errors(forge, tmp_project, args, message):
    (tmp_project / "x").mkdir()
    (tmp_project / "x" / "a.h").touch()
    before = (tmp_project / "forge.toml").read_text(encoding="utf-8")
    result = forge("deps", "add", *args)
    assert result.exit_code == 1
    assert message in result.output
    assert (tmp_project / "forge.toml").read_text(encoding="utf-8") == before


def test_vendor_add_is_a_hidden_alias(forge, tmp_project, git_calls):
    result = forge("vendor", "add", "Oryx", "glfw", "--url", "https://example.com/glfw.git", "--kind", "static-lib", "--include-subdir", "include", "--source-subdir", "src")
    assert result.exit_code == 0, result.output
    assert "`forge vendor add` is now `forge deps add`" in result.output
    assert _config(tmp_project)["dependencies"]["glfw"] == {"kind": "static", "include": "include", "sources": "src"}


def test_sync_fetches_missing_required(forge, tmp_project, git_calls):
    _populate(tmp_project, "Oryx/vendor/spdlog", "Oryx/vendor/yaml-cpp", "tests/vendor/doctest")
    result = forge("--without", "python", "deps", "sync")
    assert result.exit_code == 0, result.output
    assert git_calls == []
    result = forge("deps", "sync")
    assert [call[0][-1] for call in git_calls] == ["Oryx/vendor/pybind11"]


def test_status_shows_state_and_untracked_folders(forge, tmp_project, git_calls):
    _populate(tmp_project, "Oryx/vendor/spdlog", "Oryx/vendor/glad")
    result = forge("--without", "python", "deps", "status")
    assert result.exit_code == 0, result.output
    lines = {line.split()[1]: line for line in result.output.splitlines() if line.startswith("│")}
    assert "present" in lines["spdlog"] and "abc1234" in lines["spdlog"]
    assert "missing" in lines["yaml-cpp"]
    assert "not needed" in lines["pybind11"]
    assert "untracked: Oryx/vendor/glad → forge deps add glad --local" in result.output


def test_update_moves_a_submodule(forge, tmp_project, git_calls):
    _populate(tmp_project, "Oryx/vendor/spdlog")
    result = forge("deps", "update", "spdlog", "--rev", "v1.15.0")
    assert result.exit_code == 0, result.output
    assert [call[0] for call in git_calls[:2]] == [
        ["git", "-C", "Oryx/vendor/spdlog", "fetch", "--tags", "origin"],
        ["git", "-C", "Oryx/vendor/spdlog", "checkout", "v1.15.0"],
    ]
    assert "spdlog is now at abc1234" in result.output


def test_update_refuses_local(forge, tmp_project):
    config = tmp_project / "forge.toml"
    config.write_text(config.read_text(encoding="utf-8").replace("[docs]", 'glad = { source = "local" }\n\n[docs]'), encoding="utf-8")
    result = forge("deps", "update", "glad")
    assert result.exit_code == 1
    assert "replace the files at Oryx/vendor/glad yourself" in result.output


def test_remove_submodule(forge, tmp_project, git_calls):
    result = forge("deps", "remove", "yaml-cpp", "--yes")
    assert result.exit_code == 0, result.output
    assert [call[0] for call in git_calls] == [
        ["git", "submodule", "deinit", "-f", "--", "Oryx/vendor/yaml-cpp"],
        ["git", "rm", "-f", "--", "Oryx/vendor/yaml-cpp"],
    ]
    assert "yaml-cpp" not in _config(tmp_project)["dependencies"]


def test_remove_unknown_suggests(forge):
    result = forge("deps", "remove", "yml-cpp", "--yes")
    assert result.exit_code == 1
    assert "No dependency named 'yml-cpp' in forge.toml — did you mean 'yaml-cpp'?" in result.output


def test_remove_asks_first(forge, tmp_project, git_calls):
    result = forge("deps", "remove", "yaml-cpp")
    assert result.exit_code == 1
    assert git_calls == []
    assert "yaml-cpp" in _config(tmp_project)["dependencies"]
