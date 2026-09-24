import platform
from pathlib import Path

import pytest

from build_system import config
from build_system.config import BuildConfig, ExecutableConfig, LocalConfig


def test_defaults():
    cfg = BuildConfig()
    assert cfg.project_name == "Oryx"
    assert cfg.build_generator == "gmake"
    assert cfg.profile == "debug"
    assert cfg.test_suite == config.TestSuiteConfig(name="Tests")
    assert cfg.executables == {"oasis": ExecutableConfig(name="Oasis")}
    assert cfg.python_enabled is True
    assert cfg.sanitize is False


def test_profile_is_lowercased():
    assert BuildConfig(profile="Release").profile == "release"


def test_invalid_profile_raises():
    with pytest.raises(ValueError, match="^Invalid profile 'fast'. Must be one of: "):
        BuildConfig(profile="fast")


def test_load_missing_file_returns_defaults(tmp_path):
    assert BuildConfig.load(tmp_path / "absent.toml") == BuildConfig()


def test_load_partial_and_extended_file(tmp_path):
    path = tmp_path / "oryx.toml"
    path.write_text(
        '[build]\nprofile = "dist"\n\n[executables.bench]\nname = "Bench"\n\n[executables.oasis]\n\n[python]\nenabled = false\n',
        encoding="utf-8",
    )
    cfg = BuildConfig.load(path)
    assert cfg.profile == "dist"
    assert cfg.project_name == "Oryx"
    assert cfg.executables == {"oasis": ExecutableConfig(name="Oasis"), "bench": ExecutableConfig(name="Bench")}
    assert cfg.python_enabled is False
    assert cfg.sanitize is False


def test_load_executable_without_name_uses_its_key(tmp_path):
    path = tmp_path / "oryx.toml"
    path.write_text("[executables.demo]\n", encoding="utf-8")
    assert BuildConfig.load(path).executables["demo"] == ExecutableConfig(name="demo")


def test_load_ignores_unknown_tables(tmp_path):
    path = tmp_path / "oryx.toml"
    path.write_text('[whatever]\nkey = 1\n\n[test-suite]\nname = "Checks"\n', encoding="utf-8")
    assert BuildConfig.load(path).test_suite == config.TestSuiteConfig(name="Checks")


@pytest.mark.parametrize(
    ("machine", "profile", "token"),
    [("arm64", "debug", "debug_arm64"), ("aarch64", "release", "release_arm64"), ("x86_64", "dist", "dist_x64"), ("AMD64", "debug", "debug_x64")],
)
def test_make_config_token(monkeypatch, machine, profile, token):
    monkeypatch.setattr(platform, "machine", lambda: machine)
    assert BuildConfig(profile=profile).make_config_token == token


def test_outputdir_read_from_generated_make_file(tmp_project):
    assert BuildConfig()._outputdir_from_make() == "Debug-linux-x86_64"
    assert BuildConfig(profile="release").outputdir == "Release-linux-x86_64"


def test_outputdir_from_make_uses_project_name(tmp_project):
    assert BuildConfig(project_name="Missing")._outputdir_from_make() is None


def test_outputdir_from_make_without_matching_block(tmp_project):
    assert BuildConfig(profile="dist")._outputdir_from_make() is None
    assert BuildConfig(profile="dist").outputdir == "Dist-linux-x64"


@pytest.mark.parametrize(("system", "expected"), [("Darwin", "macosx"), ("Linux", "linux"), ("Windows", "windows"), ("FreeBSD", "freebsd")])
def test_outputdir_fallback(tmp_project, monkeypatch, system, expected):
    (tmp_project / "build" / "Oryx.make").unlink()
    monkeypatch.setattr(platform, "system", lambda: system)
    assert BuildConfig().outputdir == f"Debug-{expected}-x64"


def test_target_paths_are_nested(tmp_project):
    cfg = BuildConfig()
    bin_dir = tmp_project / "build" / "bin" / "Debug-linux-x86_64"
    assert cfg.binary_path == bin_dir
    assert cfg.executable_path("oasis") == bin_dir / "Oasis" / "Oasis"
    assert cfg.test_suite_path() == bin_dir / "Tests" / "Tests"


def test_unknown_executable_lists_available(tmp_project):
    with pytest.raises(KeyError, match="No executable named 'nope' configured. Available: oasis"):
        BuildConfig().executable_path("nope")


def test_local_config_defaults(tmp_path):
    assert LocalConfig.load(tmp_path / "absent.toml") == LocalConfig(ide_kind="none", debugger="lldb")


@pytest.mark.parametrize(("kwargs", "message"), [({"ide_kind": "emacs"}, "Invalid \\[ide\\] kind 'emacs'"), ({"debugger": "gdb"}, "Invalid \\[ide\\] debugger 'gdb'")])
def test_local_config_validation(kwargs, message):
    with pytest.raises(ValueError, match=message):
        LocalConfig(**kwargs)


def test_local_config_init_creates_then_loads(tmp_path):
    path = tmp_path / "oryx.local.toml"
    assert LocalConfig.init(path) == LocalConfig()
    path.write_text('[ide]\nkind = "vscode"\n', encoding="utf-8")
    assert LocalConfig.init(path) == LocalConfig(ide_kind="vscode")


def test_build_config_init_does_not_overwrite(tmp_path):
    path = tmp_path / "oryx.toml"
    path.write_text('[project]\nname = "Custom"\n', encoding="utf-8")
    assert BuildConfig.init(Path(path)).project_name == "Custom"
    assert path.read_text(encoding="utf-8") == '[project]\nname = "Custom"\n'
