import socket

import pytest

from pyforge.premake import install


@pytest.fixture
def no_sockets(monkeypatch):
    def refuse(*args, **kwargs):
        pytest.fail("--offline touched the network")

    monkeypatch.setattr(socket.socket, "connect", refuse)
    monkeypatch.setattr(socket, "create_connection", refuse)
    monkeypatch.setattr(socket, "getaddrinfo", refuse)


def test_offline_configure_lists_every_missing_dependency(tmp_project, forge, tmp_path, monkeypatch, no_sockets):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    commit = "a" * 40
    with (tmp_project / "forge.toml").open("a") as file:
        file.write(f'\n[dependencies.one]\nsource = "git"\nurl = "https://example.invalid/one.git"\nrev = "v1"\ncommit = "{commit}"\n')
        file.write(f'\n[dependencies.two]\nsource = "archive"\nurl = "https://example.invalid/two.tar.gz"\nsha256 = "{"b" * 64}"\n')
    result = forge("--offline", "configure")
    assert result.exit_code == 1
    assert "one" in result.output and "two" in result.output and "--offline" in result.output


def test_offline_premake_not_cached_fails_without_download(tmp_path, monkeypatch, no_sockets):
    monkeypatch.setattr(install, "install_premake", lambda *a, **k: pytest.fail("must not download"))
    assert install.ensure_premake(tmp_path / "empty", "5.0.0-beta8", "", offline=True) is None


def test_offline_install_premake_refuses(tmp_path, no_sockets):
    assert install.install_premake(tmp_path / "empty", "5.0.0-beta8", offline=True) is False
