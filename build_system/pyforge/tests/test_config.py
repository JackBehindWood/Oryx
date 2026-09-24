import tomllib

import pytest

from pyforge.config import (
    DEPENDENCY_SOURCES,
    Dependency,
    FetchMode,
    ForgeConfig,
    LocalConfig,
    Profile,
    SchemaError,
    Suite,
    load_config,
    load_local,
    parse_config,
    validate_local,
)
from pyforge.config.load import check_forge_version
from pyforge.config.schema import Choices, EditorTable, LocalBuildTable
from conftest import REAL_ROOT


def parse(text: str) -> ForgeConfig:
    return parse_config(tomllib.loads(text))


def test_minimal_config_takes_every_default():
    cfg = parse('[project]\nname = "Demo"\n')
    assert cfg.project.name == "Demo"
    assert (cfg.premake.version, cfg.premake.generator) == ("5.0.0-beta8", "gmake")
    assert (cfg.build.default_profile, cfg.build.jobs, cfg.build.fetch) == (Profile.DEBUG, 0, FetchMode.AUTO)
    assert (cfg.options, cfg.targets, cfg.dependencies, cfg.tests, cfg.docs) == ({}, {}, {}, None, None)


def test_committed_forge_toml_parses():
    cfg = load_config(REAL_ROOT / "forge.toml", "0.2.0")
    assert cfg.project.default_target in cfg.targets
    assert cfg.options["python"].default is True
    assert cfg.tool["oryx"]["stubs-dir"] == "OryxPython/stubs"


def test_enum_values_compare_as_strings():
    cfg = parse('[project]\nname = "x"\n[build]\ndefault-profile = "release"\nfetch = "never"\n')
    assert cfg.build.default_profile == "release"
    assert f"{cfg.build.fetch}" == "never"


def test_dependency_table():
    cfg = parse(
        '[project]\nname = "x"\n[options]\npython = { default = true }\n'
        '[dependencies]\nspdlog = { kind = "static", include = "include", sources = "src", defines = ["A"] }\n'
        'pybind11 = { include = "include", requires = ["python"] }\n'
    )
    assert cfg.dependencies["spdlog"] == Dependency(kind="static", include="include", sources="src", defines=["A"])
    assert cfg.dependencies["pybind11"].source == "submodule"


def test_tests_table_suites_accept_a_bare_string_or_a_table():
    cfg = parse(
        '[project]\nname = "x"\n[options]\npython = { default = true }\n[tests]\nproject = "Tests"\n'
        'suites.unit = "tests/unit"\n'
        'suites.benchmark = { dir = "tests/benchmark", default = false, requires = ["python"] }\n'
    )
    assert cfg.tests.suites["unit"] == Suite(dir="tests/unit")
    assert cfg.tests.suites["benchmark"] == Suite(dir="tests/benchmark", default=False, requires=["python"])


@pytest.mark.parametrize(
    ("text", "message"),
    [
        ("[build]\njob = 1", "unknown key 'build.job' — did you mean 'jobs'?"),
        ("[projct]\nname = 'y'", "unknown key 'projct' — did you mean 'project'?"),
        ("[build]\nfetch = 'atuo'", "'build.fetch' is 'atuo'; expected one of 'auto', 'ask', 'never' — did you mean 'auto'?"),
        ("[premake]\ngenerator = 'gmak'", "'premake.generator' is 'gmak'; expected one of 'gmake' — did you mean 'gmake'?"),
        ("[build]\njobs = true", "'build.jobs' must be an integer, not bool"),
        ("[build]\ndefault-profile = 3", "'build.default-profile' is 3; expected one of"),
        ("[targets.app]\nprojekt = 'App'", "unknown key 'targets.app.projekt' — did you mean 'project'?"),
        ("[targets.app]\nproject = 'App'\npresets = { a = 'x' }", "'targets.app.presets.a' must be an array"),
        ("[dependencies]\nx = { requires = ['pyhton'] }", "names unknown option 'pyhton'"),
        ("[tests]\nproject = 'Tests'\nsuites.x = { dir = 'd', requires = ['pyhton'] }", "'tests.suites.x.requires' names unknown option 'pyhton'"),
        ("[project]\ndefault-target = 'oasys'", "'project.default-target' is 'oasys', which is not in [targets]"),
    ],
)
def test_schema_errors(text, message):
    base = '[project]\nname = "x"\n'
    text = text.replace("[project]\n", base) if text.startswith("[project]\n") else base + text
    with pytest.raises(SchemaError) as error:
        parse(text)
    assert str(error.value).startswith("forge.toml: ")
    assert message in str(error.value)


def test_missing_project_name():
    with pytest.raises(SchemaError, match="missing required key 'project.name'"):
        parse("[project]\n")


def test_open_choices_accept_registered_names(monkeypatch):
    monkeypatch.setattr(DEPENDENCY_SOURCES, "_names", list(DEPENDENCY_SOURCES))
    with pytest.raises(SchemaError, match="did you mean 'local'"):
        parse('[project]\nname = "x"\n[dependencies]\nglad = { source = "locl" }\n')
    DEPENDENCY_SOURCES.register("conan")
    assert parse('[project]\nname = "x"\n[dependencies]\nfmt = { source = "conan" }\n').dependencies["fmt"].source == "conan"


def test_choices_register_is_idempotent():
    choices = Choices("a")
    choices.register("b")
    choices.register("b")
    assert list(choices) == ["a", "b"]


@pytest.mark.parametrize(("requirement", "installed", "ok"), [("", "0.1.0", True), (">=0.2", "0.2.0", True), (">= 0.2", "0.10.1", True), (">=0.3", "0.2.9", False)])
def test_forge_version(requirement, installed, ok):
    if ok:
        check_forge_version(requirement, installed)
    else:
        with pytest.raises(SchemaError, match="needs forge >=0.3 but 0.2.9 is installed"):
            check_forge_version(requirement, installed)


def test_forge_version_must_be_a_minimum():
    with pytest.raises(SchemaError, match="must look like"):
        check_forge_version("~=0.2", "0.2.0")


def test_local_defaults(tmp_path):
    assert load_local(tmp_path) == (LocalConfig(), None)


def test_local_file(tmp_path):
    (tmp_path / "forge.local.toml").write_text('[editor]\nkind = "vscode"\n[build]\njobs = 4\n[options]\npython = false\n', encoding="utf-8")
    local, legacy = load_local(tmp_path)
    assert legacy is None
    assert local.editor == EditorTable(kind="vscode")
    assert local.build == LocalBuildTable(jobs=4)
    assert local.options == {"python": False}


def test_legacy_local_file_is_read_and_left_alone(tmp_path):
    legacy = tmp_path / "oryx.local.toml"
    legacy.write_text('[ide]\nkind = "vscode"\ndebugger = "cppdbg"\n', encoding="utf-8")
    local, source = load_local(tmp_path)
    assert source == legacy
    assert local.editor == EditorTable(kind="vscode", debugger="cppdbg")
    assert legacy.read_text(encoding="utf-8") == '[ide]\nkind = "vscode"\ndebugger = "cppdbg"\n'
    assert not (tmp_path / "forge.local.toml").exists()


def test_forge_local_wins_over_legacy(tmp_path):
    (tmp_path / "oryx.local.toml").write_text('[ide]\nkind = "vscode"\n', encoding="utf-8")
    (tmp_path / "forge.local.toml").write_text('[editor]\nkind = "none"\n', encoding="utf-8")
    assert load_local(tmp_path) == (LocalConfig(), None)


def test_local_errors_name_their_file(tmp_path):
    (tmp_path / "forge.local.toml").write_text("[editor]\ndebuger = 'lldb'\n", encoding="utf-8")
    with pytest.raises(SchemaError, match="^forge.local.toml: unknown key 'editor.debuger' — did you mean 'debugger'"):
        load_local(tmp_path)


def test_local_options_must_exist():
    cfg = parse('[project]\nname = "x"\n[options]\npython = {}\n')
    with pytest.raises(SchemaError, match="unknown option 'pyton' in \\[options\\] — did you mean 'python'"):
        validate_local(LocalConfig(options={"pyton": False}), cfg)


def test_invalid_toml_names_the_file(tmp_path):
    (tmp_path / "forge.local.toml").write_text("[editor\n", encoding="utf-8")
    with pytest.raises(SchemaError, match="^forge.local.toml: "):
        load_local(tmp_path)
