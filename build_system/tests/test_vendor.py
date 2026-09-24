import pytest
import typer

from build_system import vendor


def _populate(root, *dirs):
    for name in dirs:
        (root / name / "README").touch()


def test_vendor_dirs_are_sorted_per_project_and_skip_premake(tmp_project):
    (tmp_project / "Oryx/vendor/premake").mkdir()
    (tmp_project / "Oryx/vendor/notes.txt").touch()
    assert [p.relative_to(tmp_project).as_posix() for p in vendor.vendor_dirs(tmp_project)] == [
        "Oryx/vendor/pybind11",
        "Oryx/vendor/spdlog",
        "Oryx/vendor/yaml-cpp",
        "tests/vendor/doctest",
    ]


def test_missing_vendor_dirs_are_the_empty_ones(tmp_project):
    _populate(tmp_project, "Oryx/vendor/spdlog", "tests/vendor/doctest")
    names = [p.name for p in vendor.missing_vendor_dirs(tmp_project)]
    assert names == ["pybind11", "yaml-cpp"]


def test_missing_vendor_dirs_skip_pybind11_without_python(tmp_project):
    _populate(tmp_project, "Oryx/vendor/spdlog", "tests/vendor/doctest")
    assert [p.name for p in vendor.missing_vendor_dirs(tmp_project, python_enabled=False)] == ["yaml-cpp"]


def test_missing_vendor_dirs_without_config_checks_everything(tmp_project):
    assert len(vendor.missing_vendor_dirs(tmp_project)) == 4


def test_include_paths(tmp_project):
    (tmp_project / "Oryx/vendor/spdlog/include").mkdir()
    (tmp_project / "Oryx/vendor/pybind11/include").mkdir()
    (tmp_project / "tests/vendor/doctest/doctest").mkdir()
    assert vendor.vendor_include_paths(tmp_project) == [
        "${workspaceFolder}/Oryx/vendor/pybind11/include",
        "${workspaceFolder}/Oryx/vendor/spdlog/include",
        "${workspaceFolder}/Oryx/vendor/yaml-cpp",
        "${workspaceFolder}/tests/vendor/doctest",
        "${workspaceFolder}/tests/vendor/doctest/doctest",
    ]
    assert "${workspaceFolder}/Oryx/vendor/pybind11/include" not in vendor.vendor_include_paths(tmp_project, python_enabled=False)


def test_ensure_vendor_dirs_passes_when_populated(tmp_project):
    _populate(tmp_project, *(p.relative_to(tmp_project) for p in vendor.vendor_dirs(tmp_project)))
    vendor.ensure_vendor_dirs(tmp_project)


def test_ensure_vendor_dirs_exits(tmp_project):
    with pytest.raises(typer.Exit) as error:
        vendor.ensure_vendor_dirs(tmp_project)
    assert error.value.exit_code == 1
