import tomllib

from test_dep_git import _git, remote  # noqa: F401


def _entry(root, name):
    return tomllib.loads((root / "forge.toml").read_text())["dependencies"][name]


def test_glad_walkthrough(tmp_project, forge):
    glad = tmp_project / "Oryx" / "vendor" / "glad"
    (glad / "include" / "glad").mkdir(parents=True)
    (glad / "include" / "glad" / "glad.h").touch()
    (glad / "src").mkdir()
    (glad / "src" / "glad.c").touch()
    result = forge("deps", "add", "glad", "--local")
    assert result.exit_code == 0, result.output
    assert _entry(tmp_project, "glad") == {"source": "local", "kind": "static", "include": "include", "sources": "src"}


def test_git_walkthrough(tmp_project, forge, remote):  # noqa: F811
    url, first, head = remote
    result = forge("deps", "add", "glfw", "--git", url, "--rev", "v1", "--define", "GLFW_STATIC")
    assert result.exit_code == 0, result.output
    entry = _entry(tmp_project, "glfw")
    assert entry == {"source": "git", "url": url, "rev": "v1", "commit": first, "defines": ["GLFW_STATIC"]}
