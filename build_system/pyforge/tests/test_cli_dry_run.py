import sys

import pytest
import typer

from conftest import workspace_json
from pyforge.premake.install import lua_scripts_dir

PYTHON_OPTIONS = "--python-include=/py/include/python3.11 --python-libdir=/py/lib --python-lib=python3.11"
SCRIPTS_FLAG = f"--scripts={lua_scripts_dir()}"


@pytest.fixture
def dry(forge):
    def run(*args: str) -> list[str]:
        result = forge("--dry-run", *args)
        assert result.exit_code == 0, result.output
        return result.output.splitlines()

    return run


@pytest.fixture
def premake(tmp_project):
    return tmp_project / "cache" / "premake" / "5.0.0-beta8" / "premake5"


@pytest.fixture
def tests_binary(tmp_project):
    return tmp_project / "build" / "bin" / "Debug-linux-x86_64" / "Tests" / "Tests"


def test_rich_swallows_the_dry_run_prefix_as_markup(dry, premake):
    assert dry("configure") == [f" would run: {premake} gmake {SCRIPTS_FLAG} {PYTHON_OPTIONS} --forge-export"]


@pytest.mark.parametrize(
    ("flags", "options"),
    [
        ([], PYTHON_OPTIONS),
        (["--without", "python"], "--no-python"),
        (["--with", "sanitize", "--without", "python"], "--no-python --sanitize"),
        (["--without", "python", "-D", "cc=clang", "-D", "verbose"], "--no-python --cc=clang --verbose"),
    ],
)
def test_configure(dry, premake, flags, options):
    assert dry(*flags, "configure") == [f" would run: {premake} gmake {SCRIPTS_FLAG} {options} --forge-export"]


@pytest.mark.parametrize(
    ("flags", "token"),
    [
        ([], "debug_x64"),
        (["--profile", "release"], "release_x64"),
        (["--profile", "DIST"], "dist_x64"),
        (["--without", "python"], "debug_x64"),
        (["--with", "sanitize"], "debug_x64"),
    ],
)
def test_compile(dry, tmp_project, flags, token):
    assert dry(*flags, "compile") == [f" would run: make -C {tmp_project / 'build'} -j8 {'config=' + token}"]


def test_clean(dry, tmp_project):
    assert dry("clean") == [f" would remove: {tmp_project / 'build'}"]


def test_all_runs_configure_compile_test_in_order(dry, tmp_project, premake, tests_binary):
    assert dry("all") == [
        f" would run: {premake} gmake {SCRIPTS_FLAG} {PYTHON_OPTIONS} --forge-export",
        f" would run: make -C {tmp_project / 'build'} -j8 config=debug_x64",
        f" would run: {tests_binary} --source-file=*tests/unit/*,*tests/integration/*",
    ]


@pytest.mark.parametrize(
    ("args", "passed"),
    [
        ([], ""),
        (["oasis"], ""),
        (["oasis:bench"], " --simulate=random,first-legal,100 --benchmark"),
        (["oasis", "--", "--game=tictactoe", "--opponent", "human"], " --game=tictactoe --opponent human"),
        (["--", "--game=tictactoe"], " --game=tictactoe"),
        (["oasis:bench", "--", "--seed=4"], " --simulate=random,first-legal,100 --benchmark --seed=4"),
        (["--profile", "release"], ""),
    ],
)
def test_run(dry, tmp_project, args, passed):
    profile = "Release" if "release" in args else "Debug"
    global_flags = args[:2] if "--profile" in args else []
    command_args = args[2:] if global_flags else args
    oasis = tmp_project / "build" / "bin" / f"{profile}-linux-x86_64" / "Oasis" / "Oasis"
    assert dry(*global_flags, "run", *command_args) == [f" would run: {oasis}{passed}"]


@pytest.mark.parametrize(
    ("args", "message"),
    [
        (["oasys"], "Unknown target 'oasys' — did you mean 'oasis'? (available: oasis, core)"),
        (["oasis:bnech"], "Unknown preset 'oasis:bnech' — did you mean 'bench'? (available: bench)"),
        (["core"], "Target 'core' is Premake project 'Oryx', a StaticLib, not an executable."),
    ],
)
def test_run_errors(forge, tmp_project, args, message):
    config = tmp_project / "forge.toml"
    config.write_text(config.read_text(encoding="utf-8") + '\n[targets.core]\nproject = "Oryx"\n', encoding="utf-8")
    result = forge("--dry-run", "run", *args)
    assert result.exit_code == 1
    assert message in result.output


def test_run_from_the_menu_keeps_the_menu_process(tmp_project, run, monkeypatch):
    from pyforge.commands import build

    binary = _fake_binary(tmp_project)
    run.interactive = True
    calls = []
    monkeypatch.setattr(build, "_can_replace_process", lambda: True)
    monkeypatch.setattr(build.os, "execv", lambda *args: calls.append("execv"))
    monkeypatch.setattr(build.subprocess, "run", lambda argv, cwd: calls.append(argv) or build.subprocess.CompletedProcess(argv, 0))
    build._exec([str(binary)], tmp_project, replace_process=not run.interactive)
    assert calls == [[str(binary)]]


def test_run_without_a_default_target(forge, tmp_project):
    config = tmp_project / "forge.toml"
    config.write_text(config.read_text(encoding="utf-8").replace('default-target = "oasis"\n', ""), encoding="utf-8")
    result = forge("--dry-run", "run")
    assert result.exit_code == 1
    assert "no [project] default-target" in result.output


def _fake_binary(tmp_project):
    binary = tmp_project / "build" / "bin" / "Debug-linux-x86_64" / "Oasis" / "Oasis"
    binary.parent.mkdir(parents=True)
    binary.touch()
    return binary


def test_run_replaces_the_process_from_the_project_root(forge, tmp_project, monkeypatch):
    from pyforge.commands import build

    binary = _fake_binary(tmp_project)
    calls = []
    monkeypatch.setattr(build, "_can_replace_process", lambda: True)
    monkeypatch.setattr(build.os, "execv", lambda path, argv: calls.append((path, argv, build.os.getcwd())))
    (tmp_project / "sub").mkdir()
    monkeypatch.chdir(tmp_project / "sub")
    result = forge("run", "oasis:bench")
    assert result.exit_code == 0, result.output
    assert calls == [(str(binary), [str(binary), "--simulate=random,first-legal,100", "--benchmark"], str(tmp_project))]


def test_run_uses_a_subprocess_on_windows(forge, tmp_project, monkeypatch):
    from pyforge.commands import build

    binary = _fake_binary(tmp_project)
    calls = []
    monkeypatch.setattr(build, "_can_replace_process", lambda: False)
    monkeypatch.setattr(build.subprocess, "run", lambda argv, cwd: calls.append((argv, cwd)) or build.subprocess.CompletedProcess(argv, 3))
    result = forge("run", "--", "--game=x")
    assert result.exit_code == 3
    assert calls == [([str(binary), "--game=x"], tmp_project)]


def test_run_missing_binary(forge):
    result = forge("run")
    assert result.exit_code == 1
    assert "Executable missing at:" in result.output


def test_run_before_configure_asks_for_it(forge, tmp_project):
    (tmp_project / "build" / "forge" / "workspace.json").unlink()
    result = forge("--dry-run", "run")
    assert result.exit_code == 1
    assert "run `forge configure` first" in result.output


def test_compile_before_configure_uses_a_placeholder_token(dry, tmp_project):
    (tmp_project / "build" / "forge" / "workspace.json").unlink()
    assert dry("compile") == [f" would run: make -C {tmp_project / 'build'} -j8 config=<from export>"]


def test_tests(dry, tests_binary):
    assert dry("test") == [f" would run: {tests_binary} --source-file=*tests/unit/*,*tests/integration/*"]


def test_tests_run_alias_is_deprecated(dry, tests_binary):
    assert dry("test", "run") == [
        "Note: 'test run' is deprecated; use 'forge test'.",
        f" would run: {tests_binary} --source-file=*tests/unit/*,*tests/integration/*",
    ]


def test_benchmark(dry, tests_binary):
    assert dry("test", "benchmark") == [f" would run: {tests_binary} --source-file=*tests/benchmark/*"]


def test_tests_unknown_suite(forge):
    result = forge("--dry-run", "test", "unti")
    assert result.exit_code == 1
    assert "Unknown test suite 'unti' — did you mean 'unit'?" in result.output


def test_tests_passthrough_rejected_across_runners(forge, tmp_project):
    (tmp_project / "tests" / "python").mkdir(parents=True)
    (tmp_project / "tests" / "python" / "conftest.py").touch()
    config = tmp_project / "forge.toml"
    config.write_text(
        config.read_text(encoding="utf-8").replace(
            'suites.benchmark = { dir = "tests/benchmark", default = false }',
            'suites.benchmark = { dir = "tests/benchmark", default = false }\nsuites.python = { dir = "tests/python" }',
        ),
        encoding="utf-8",
    )
    result = forge("--dry-run", "test", "unit,python", "--", "-k", "foo")
    assert result.exit_code == 1
    assert "-- arguments need every selected suite to share one runner" in result.output


def test_docs(dry, tmp_project):
    mkdocs = tmp_project / "mkdocs.yml"
    assert dry("docs", "build") == [f" would run: {sys.executable} -m mkdocs build --strict --site-dir {tmp_project / 'site'} -f {mkdocs}"]
    assert dry("docs", "serve", "--port", "9000") == [
        f" would run: {sys.executable} -m mkdocs serve --dev-addr localhost:9000 -f {mkdocs}"
    ]
    assert dry("docs", "clean") == [f" would remove: {tmp_project / 'site'}"]


def test_python_stubs(dry):
    assert dry("python", "stubs") == [f" would run: {sys.executable} -m pybind11_stubgen oryx -o OryxPython/stubs"]


def test_docs_follow_the_docs_table(dry, tmp_project):
    from pyforge import tomledit

    tomledit.edit_file(tmp_project / "forge.toml", lambda text: tomledit.set_value(tomledit.set_value(text, ["docs", "config"], "site.yml"), ["docs", "site"], "public"))
    assert dry("docs", "build") == [f" would run: {sys.executable} -m mkdocs build --strict --site-dir {tmp_project / 'public'} -f {tmp_project / 'site.yml'}"]
    assert dry("docs", "clean") == [f" would remove: {tmp_project / 'public'}"]


def test_missing_mkdocs_names_uv_and_pip(forge, monkeypatch):
    import importlib.util

    real_find_spec = importlib.util.find_spec
    monkeypatch.setattr(importlib.util, "find_spec", lambda name, *args: None if name == "mkdocs" else real_find_spec(name, *args))
    result = forge("docs", "build")
    assert result.exit_code == 1
    output = " ".join(result.output.split())
    assert "uv sync --group docs" in output and "-m pip install mkdocs mkdocs-material" in output


def test_python_stubs_needs_a_stubs_dir(forge, tmp_project):
    config = tmp_project / "forge.toml"
    config.write_text(config.read_text(encoding="utf-8").replace('stubs-dir = "OryxPython/stubs"\n', ""), encoding="utf-8")
    result = forge("--dry-run", "python", "stubs")
    assert result.exit_code == 1
    assert "stubs-dir" in result.output


def test_premake_status_ignores_dry_run(dry, premake):
    assert dry("premake", "status") == [
        f"✗ premake5: not installed locally (expected {premake}).",
        "  Run 'forge configure' or 'forge premake install' to install it.",
    ]


def test_init_ignores_dry_run_and_keeps_existing_file(dry, tmp_project):
    before = (tmp_project / "forge.toml").read_text(encoding="utf-8")
    assert dry("init")[0] == f"⚠️ Configuration file already exists at: {tmp_project / 'forge.toml'}"
    assert (tmp_project / "forge.toml").read_text(encoding="utf-8") == before
    assert not (tmp_project / "forge.local.toml").exists()


def test_init_dry_run_creates_nothing_in_a_fresh_project(forge, tmp_project):
    (tmp_project / "forge.toml").unlink()
    result = forge("--dry-run", "init", "--yes")
    assert result.exit_code == 0, result.output
    assert f"would create: {tmp_project / 'forge.toml'}" in result.output
    assert not (tmp_project / "forge.toml").exists()


def test_init_creates_missing_file(forge, tmp_project):
    (tmp_project / "forge.toml").unlink()
    result = forge("init", "--yes")
    assert result.exit_code == 0, result.output
    assert (tmp_project / "forge.toml").read_text(encoding="utf-8") == f'[project]\nname = "{tmp_project.name}"\n'
    assert "No premake5.lua found" in result.output


def test_init_scaffolds_an_app_template(forge, tmp_project, monkeypatch):
    from pyforge.commands import build

    def no_premake(ctx):
        raise typer.Exit(code=1)

    (tmp_project / "forge.toml").unlink()
    monkeypatch.setattr(build, "configure", no_premake)
    result = forge("init", "--yes", "--template", "app")
    assert result.exit_code == 0, result.output
    assert (tmp_project / "premake5.lua").is_file()
    assert (tmp_project / "src" / "main.cpp").is_file()
    assert "couldn't run Premake" in result.output


def test_init_derives_targets_from_an_existing_premake_export(forge, tmp_project, monkeypatch):
    from pyforge.commands import build

    (tmp_project / "forge.toml").unlink()
    (tmp_project / "premake5.lua").write_text("-- pretend project\n", encoding="utf-8")
    monkeypatch.setattr(
        build,
        "configure",
        lambda ctx: (tmp_project / "build" / "forge" / "workspace.json").write_text(workspace_json(tmp_project), encoding="utf-8"),
    )
    result = forge("init", "--yes")
    assert result.exit_code == 0, result.output
    config = tmp_project / "forge.toml"
    assert '[targets.oasis]\nproject = "Oasis"' in config.read_text(encoding="utf-8")
    assert '[targets.tests]\nproject = "Tests"' in config.read_text(encoding="utf-8")
    assert 'default-target = "oasis"' in config.read_text(encoding="utf-8")


def test_init_rejects_an_unknown_template(forge, tmp_project):
    (tmp_project / "forge.toml").unlink()
    result = forge("init", "--yes", "--template", "bogus")
    assert result.exit_code == 1
    assert "Unknown --template 'bogus'" in result.output


def test_editor_vscode_remembers_the_debugger_choice(forge, tmp_project):
    assert forge("editor", "vscode", "--debugger", "cppdbg").exit_code == 0
    assert (tmp_project / "forge.local.toml").read_text(encoding="utf-8") == '[editor]\nkind = "vscode"\ndebugger = "cppdbg"\n'


def test_editor_vscode_copies_the_legacy_local_file_and_leaves_it(forge, tmp_project):
    legacy = tmp_project / "oryx.local.toml"
    legacy.write_text('[ide]\nkind = "vscode"\ndebugger = "lldb"\n', encoding="utf-8")
    result = forge("editor", "vscode", "--debugger", "cppdbg")
    assert result.exit_code == 0, result.output
    assert "rename it to forge.local.toml" in result.output
    assert (tmp_project / "forge.local.toml").read_text(encoding="utf-8") == '[editor]\nkind = "vscode"\ndebugger = "cppdbg"\n'
    assert legacy.read_text(encoding="utf-8") == '[ide]\nkind = "vscode"\ndebugger = "lldb"\n'


def test_editor_vscode_writes_four_files(forge, tmp_project):
    result = forge("editor", "vscode", "--no-remember")
    assert result.exit_code == 0, result.output
    assert sorted(p.name for p in (tmp_project / ".vscode").iterdir()) == [
        "c_cpp_properties.json",
        "launch.json",
        "settings.json",
        "tasks.json",
    ]


def test_bare_forge_prints_help_outside_a_tty(forge):
    result = forge()
    assert result.exit_code == 0
    for name in ("configure", "compile", "all", "clean", "run", "config", "deps", "docs", "editor", "python", "premake", "target", "init", "test"):
        assert name in result.output
    assert "vendor" not in result.output


def test_invalid_profile_is_a_configuration_error(forge):
    result = forge("--profile", "bogus", "compile")
    assert result.exit_code == 1
    assert result.output.startswith("Configuration Error: command line: '--profile' is 'bogus'; expected one of 'debug', 'release', 'dist'")


def test_schema_error_is_a_configuration_error(forge, tmp_project):
    (tmp_project / "forge.toml").write_text('[project]\nname = "x"\n[build]\njob = 2\n', encoding="utf-8")
    result = forge("compile")
    assert result.exit_code == 1
    assert "unknown key 'build.job' — did you mean 'jobs'?" in result.output


def test_runs_from_a_subdirectory(forge, tmp_project, monkeypatch):
    (tmp_project / "Oasis" / "src").mkdir(parents=True)
    monkeypatch.chdir(tmp_project / "Oasis" / "src")
    result = forge("--dry-run", "clean")
    assert result.output.strip() == f"would remove: {tmp_project / 'build'}"


@pytest.mark.parametrize(
    ("flags", "message"),
    [
        (["--with", "sanitise"], "--with sanitise: unknown option — did you mean 'sanitize'? (available: python, sanitize)"),
        (["--without", "gui"], "--without gui: unknown option (available: python, sanitize)"),
        (["-D", "=x"], "-D '=x': expected KEY or KEY=VALUE"),
    ],
)
def test_bad_option_flags_are_configuration_errors(forge, flags, message):
    result = forge(*flags, "configure")
    assert result.exit_code == 1
    assert message in result.output


def test_local_options_override_the_defaults(dry, premake, tmp_project):
    (tmp_project / "forge.local.toml").write_text("[options]\npython = false\n", encoding="utf-8")
    assert dry("configure") == [f" would run: {premake} gmake {SCRIPTS_FLAG} --no-python --forge-export"]
    assert dry("--with", "python", "configure") == [f" would run: {premake} gmake {SCRIPTS_FLAG} {PYTHON_OPTIONS} --forge-export"]


def test_no_python_and_sanitize_aliases_are_gone(forge):
    output = forge("--help").output
    assert "--with" in output and "--without" in output
    assert "--no-python" not in output and "--sanitize" not in output
    result = forge("--no-python", "configure")
    assert result.exit_code != 0


def test_editor_vscode_configures_first_without_an_export(forge, tmp_project, monkeypatch):
    from pyforge.commands import build

    (tmp_project / "build" / "forge" / "workspace.json").unlink()
    calls = []
    monkeypatch.setattr(build, "configure", lambda ctx: calls.append("configure") or (tmp_project / "build" / "forge" / "workspace.json").write_text(workspace_json(tmp_project), encoding="utf-8"))
    result = forge("editor", "vscode", "--no-remember")
    assert result.exit_code == 0, result.output
    assert calls == ["configure"]
    assert (tmp_project / ".vscode" / "launch.json").is_file()


def test_editor_vs2022_dry_run(dry, premake):
    assert dry("editor", "vs2022") == [f" would run: {premake} vs2022 {SCRIPTS_FLAG} {PYTHON_OPTIONS}"]
