import pytest

from pyforge import cache
from pyforge.config import Dependency
from pyforge.deps.resolve import DependencyError, ResolvedDependency
from conftest import _git
from pyforge.deps.sources.git import GitSource, default_branch, install_clone


def _dep(url, rev, commit):
    return ResolvedDependency("lib", Dependency(source="git", url=url, rev=rev, commit=commit), cache.deps_dir("lib", commit))


def test_default_branch_and_tag_rev(remote, tmp_path):
    url, first, head = remote
    assert default_branch(url) == "main"
    assert install_clone(tmp_path, "lib", url, "v1") == first
    assert (cache.deps_dir("lib", first) / "a.h").read_text() == "1"


def test_commit_rev_and_fetch_pin(remote, tmp_path):
    url, first, head = remote
    assert install_clone(tmp_path, "lib", url, first) == first
    dep = _dep(url, "main", head)
    GitSource().fetch(tmp_path, dep)
    assert dep.present
    assert GitSource().pin(tmp_path, dep) == head[:12]
    assert cache.read_pins()[str(tmp_path)]["lib"] == head


def test_wrong_commit_in_toml_is_a_pin_mismatch(remote, tmp_path):
    url, first, head = remote
    with pytest.raises(DependencyError):
        GitSource().fetch(tmp_path, _dep(url, "main", "0" * 40))
    install_clone(tmp_path, "lib", url, first)
    corrupt = ResolvedDependency("lib", Dependency(source="git", url=url, rev="v1", commit=head), cache.deps_dir("lib", first))
    with pytest.raises(DependencyError, match="not the pinned"):
        GitSource().pin(tmp_path, corrupt)


def test_cli_add_and_update_git(remote, tmp_project, forge):
    url, first, head = remote
    result = forge("deps", "add", "lib", "--git", url, "--rev", "v1")
    assert result.exit_code == 0, result.output
    text = (tmp_project / "forge.toml").read_text()
    assert f'commit = "{first}"' in text and 'rev = "v1"' in text
    result = forge("deps", "update", "lib")
    assert result.exit_code == 0, result.output
    text = (tmp_project / "forge.toml").read_text()
    assert f'commit = "{head}"' in text and 'rev = "main"' in text


@pytest.mark.parametrize("url, rev", [("--upload-pack=touch /tmp/pwned", "main"), ("file:///x", "--upload-pack=x")])
def test_option_like_url_or_rev_is_refused(tmp_path, monkeypatch, url, rev):
    monkeypatch.setenv("PYFORGE_CACHE", str(tmp_path / "cache"))
    with pytest.raises(DependencyError, match="starting with '-'"):
        install_clone(tmp_path, "lib", url, rev)
    with pytest.raises(DependencyError, match="starting with '-'"):
        default_branch("--upload-pack=x")
