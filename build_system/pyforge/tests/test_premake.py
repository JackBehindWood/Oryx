import subprocess

import pytest

from pyforge.premake import install as premake


def test_installed_version_runs_outside_repo_root(tmp_path, monkeypatch):
    executable = tmp_path / "premake5"
    executable.touch()
    calls = []

    def fake_run(command, **kwargs):
        calls.append(kwargs)
        return subprocess.CompletedProcess(command, 0, stdout="premake5 (Premake Build Script Generator) 5.0.0-beta8\n")

    monkeypatch.setattr(premake.subprocess, "run", fake_run)

    assert premake.installed_version(executable).endswith("5.0.0-beta8")
    assert calls[0]["cwd"] == tmp_path
    assert not (tmp_path / "premake5.lua").exists()


def _linux_arm64(monkeypatch):
    monkeypatch.setattr("platform.system", lambda: "Linux")
    monkeypatch.setattr("platform.machine", lambda: "aarch64")


def _fake_premake_on_path(tmp_path, monkeypatch):
    bin_dir = tmp_path / "on-path"
    bin_dir.mkdir()
    fake = bin_dir / "premake5"
    fake.write_text("#!/bin/sh\necho 'premake5 (Premake Build Script Generator) 5.0.0-beta8'\n", encoding="utf-8")
    fake.chmod(0o755)
    monkeypatch.setenv("PATH", str(bin_dir))
    return fake


def test_linux_arm64_has_no_release_asset():
    assert premake.asset_name("Linux", "aarch64") is None
    assert premake.asset_name("Linux", "x86_64") == "linux.tar.gz"


def test_linux_arm64_falls_back_to_premake_on_path(tmp_path, monkeypatch):
    _linux_arm64(monkeypatch)
    fake = _fake_premake_on_path(tmp_path, monkeypatch)
    monkeypatch.setattr(premake, "install_premake", lambda *args, **kwargs: pytest.fail("must not download"))

    assert premake.ensure_premake(tmp_path / "cache") == fake


def test_linux_arm64_without_premake_on_path_fails_with_the_fix(tmp_path, monkeypatch, capsys):
    _linux_arm64(monkeypatch)
    monkeypatch.setenv("PATH", str(tmp_path))

    assert premake.ensure_premake(tmp_path / "cache") is None
    assert "set [premake] path" in capsys.readouterr().out


def test_premake_path_override_beats_the_system_fallback(tmp_path, monkeypatch):
    _linux_arm64(monkeypatch)
    _fake_premake_on_path(tmp_path, monkeypatch)
    assert premake.system_premake("tools/premake") is None


def test_status_reports_the_system_premake(forge, tmp_project, tmp_path, monkeypatch):
    _linux_arm64(monkeypatch)
    fake = _fake_premake_on_path(tmp_path, monkeypatch)
    result = forge("premake", "status")
    assert f"using system premake5 at {fake}" in result.output.replace("\n", "")
