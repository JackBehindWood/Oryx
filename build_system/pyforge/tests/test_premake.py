import subprocess

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
