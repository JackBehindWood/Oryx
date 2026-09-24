import pytest

from pyforge.project import Project, ProjectNotFound, find_root


def test_find_root_walks_up_from_a_subdirectory(tmp_path):
    (tmp_path / "forge.toml").touch()
    nested = tmp_path / "Oasis" / "src"
    nested.mkdir(parents=True)
    assert find_root(nested) == tmp_path.resolve()


def test_forge_toml_wins_over_legacy_name(tmp_path):
    (tmp_path / "oryx.toml").touch()
    (tmp_path / "forge.toml").touch()
    assert Project.discover(tmp_path).config_file == tmp_path.resolve() / "forge.toml"


def test_legacy_name_is_found(tmp_path):
    (tmp_path / "oryx.toml").touch()
    assert Project.discover(tmp_path).config_file == tmp_path.resolve() / "oryx.toml"


def test_nearest_config_wins(tmp_path):
    (tmp_path / "forge.toml").touch()
    inner = tmp_path / "vendored"
    inner.mkdir()
    (inner / "forge.toml").touch()
    assert find_root(inner) == inner.resolve()


def test_missing_config_names_forge_init(tmp_path):
    with pytest.raises(ProjectNotFound, match="forge init"):
        find_root(tmp_path)


def test_paths(tmp_path):
    project = Project.from_config(tmp_path / "forge.toml")
    assert project.root == tmp_path.resolve()
    assert project.forge_dir == tmp_path.resolve() / "build" / "forge"
