import pytest

PYTHON_OPTIONS = "--python-include=/py/include/python3.11 --python-libdir=/py/lib --python-lib=python3.11"


@pytest.fixture
def dry(forge):
    def run(*args: str) -> list[str]:
        result = forge("--dry-run", *args)
        assert result.exit_code == 0, result.output
        return result.output.splitlines()

    return run


@pytest.fixture
def premake(tmp_project):
    return tmp_project / "premake" / "bin" / "premake5"


@pytest.fixture
def tests_binary(tmp_project):
    return tmp_project / "build" / "bin" / "Debug-linux-x86_64" / "Tests" / "Tests"


def test_rich_swallows_the_dry_run_prefix_as_markup(dry, premake):
    assert dry("build", "configure") == [f" would run: {premake} gmake {PYTHON_OPTIONS} --forge-export"]


@pytest.mark.parametrize(
    ("flags", "options"),
    [
        ([], PYTHON_OPTIONS),
        (["--no-python"], "--no-python"),
        (["--sanitize"], f"--sanitize {PYTHON_OPTIONS}"),
        (["--no-python", "--sanitize"], "--no-python --sanitize"),
        (["--without", "python"], "--no-python"),
        (["--with", "sanitize", "--without", "python"], "--no-python --sanitize"),
        (["--without", "python", "-D", "cc=clang", "-D", "verbose"], "--no-python --cc=clang --verbose"),
    ],
)
def test_configure(dry, premake, flags, options):
    assert dry(*flags, "build", "configure") == [f" would run: {premake} gmake {options} --forge-export"]


@pytest.mark.parametrize(
    ("flags", "token"),
    [
        ([], "debug_x64"),
        (["--profile", "release"], "release_x64"),
        (["--profile", "DIST"], "dist_x64"),
        (["--no-python"], "debug_x64"),
        (["--sanitize"], "debug_x64"),
        (["--no-python", "--sanitize"], "debug_x64"),
    ],
)
def test_compile(dry, tmp_project, flags, token):
    assert dry(*flags, "build", "compile") == [f" would run: make -C {tmp_project / 'build'} -j8 {'config=' + token}"]


def test_clean(dry, tmp_project):
    assert dry("build", "clean") == [f" would remove: {tmp_project / 'build'}"]


def test_all_runs_configure_compile_test_in_order(dry, tmp_project, premake, tests_binary):
    assert dry("build", "all") == [
        f" would run: {premake} gmake {PYTHON_OPTIONS} --forge-export",
        f" would run: make -C {tmp_project / 'build'} -j8 config=debug_x64",
        f" would run: {tests_binary}",
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
    assert dry(*global_flags, "build", "run", *command_args) == [f" would run: {oasis}{passed}"]


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
    result = forge("--dry-run", "build", "run", *args)
    assert result.exit_code == 1
    assert message in result.output


def test_run_from_the_menu_keeps_the_menu_process(tmp_project, run, monkeypatch):
    from build_system.commands import build

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
    result = forge("--dry-run", "build", "run")
    assert result.exit_code == 1
    assert "no [project] default-target" in result.output


def _fake_binary(tmp_project):
    binary = tmp_project / "build" / "bin" / "Debug-linux-x86_64" / "Oasis" / "Oasis"
    binary.parent.mkdir(parents=True)
    binary.touch()
    return binary


def test_run_replaces_the_process_from_the_project_root(forge, tmp_project, monkeypatch):
    from build_system.commands import build

    binary = _fake_binary(tmp_project)
    calls = []
    monkeypatch.setattr(build, "_can_replace_process", lambda: True)
    monkeypatch.setattr(build.os, "execv", lambda path, argv: calls.append((path, argv, build.os.getcwd())))
    (tmp_project / "sub").mkdir()
    monkeypatch.chdir(tmp_project / "sub")
    result = forge("build", "run", "oasis:bench")
    assert result.exit_code == 0, result.output
    assert calls == [(str(binary), [str(binary), "--simulate=random,first-legal,100", "--benchmark"], str(tmp_project))]


def test_run_uses_a_subprocess_on_windows(forge, tmp_project, monkeypatch):
    from build_system.commands import build

    binary = _fake_binary(tmp_project)
    calls = []
    monkeypatch.setattr(build, "_can_replace_process", lambda: False)
    monkeypatch.setattr(build.subprocess, "run", lambda argv, cwd: calls.append((argv, cwd)) or build.subprocess.CompletedProcess(argv, 3))
    result = forge("build", "run", "--", "--game=x")
    assert result.exit_code == 3
    assert calls == [([str(binary), "--game=x"], tmp_project)]


def test_run_missing_binary(forge):
    result = forge("build", "run")
    assert result.exit_code == 1
    assert "Executable missing at:" in result.output


def test_run_before_configure_asks_for_it(forge, tmp_project):
    (tmp_project / "build" / "forge" / "workspace.json").unlink()
    result = forge("--dry-run", "build", "run")
    assert result.exit_code == 1
    assert "run `forge build configure` first" in result.output


def test_compile_before_configure_uses_a_placeholder_token(dry, tmp_project):
    (tmp_project / "build" / "forge" / "workspace.json").unlink()
    assert dry("build", "compile") == [f" would run: make -C {tmp_project / 'build'} -j8 config=<from export>"]


@pytest.mark.parametrize("args", [["test"], ["test", "run"]])
def test_tests(dry, tests_binary, args):
    assert dry(*args) == [f" would run: {tests_binary}"]


def test_benchmark(dry, tests_binary):
    assert dry("test", "benchmark") == [f" would run: {tests_binary} --test-suite=benchmark"]


def test_docs(dry, tmp_project):
    mkdocs = tmp_project / "mkdocs.yml"
    assert dry("docs", "build") == [f" would run: uv run --group docs mkdocs build --strict -f {mkdocs}"]
    assert dry("docs", "serve", "--port", "9000") == [
        f" would run: uv run --group docs mkdocs serve --dev-addr localhost:9000 -f {mkdocs}"
    ]
    assert dry("docs", "clean") == [f" would remove: {tmp_project / 'site'}"]


def test_python_stubs(dry):
    assert dry("python", "stubs") == [" would run: uv run --group stubs python -m pybind11_stubgen oryx -o OryxPython/stubs"]


def test_setup_premake_ignores_dry_run(dry, premake):
    assert dry("setup", "premake") == [
        f"✗ premake5: not installed locally (expected {premake}).",
        "  Run 'forge build configure' or 'forge setup premake --update' to install it.",
    ]


def test_config_init_ignores_dry_run_and_keeps_existing_file(dry, tmp_project):
    before = (tmp_project / "forge.toml").read_text(encoding="utf-8")
    assert dry("config", "init", "--no-remember")[1] == f"⚠️ Configuration file already exists at: {tmp_project / 'forge.toml'}"
    assert (tmp_project / "forge.toml").read_text(encoding="utf-8") == before
    assert not (tmp_project / "forge.local.toml").exists()


def test_config_init_creates_missing_file(forge, tmp_project):
    (tmp_project / "forge.toml").unlink()
    result = forge("config", "init", "--no-remember")
    assert result.exit_code == 0, result.output
    assert (tmp_project / "forge.toml").read_text(encoding="utf-8") == f'[project]\nname = "{tmp_project.name}"\n'


def test_config_init_remembers_ide_choice(forge, tmp_project):
    assert forge("config", "init", "--debugger", "cppdbg").exit_code == 0
    assert (tmp_project / "forge.local.toml").read_text(encoding="utf-8") == '[editor]\nkind = "none"\ndebugger = "cppdbg"\n'


def test_config_init_copies_the_legacy_local_file_and_leaves_it(forge, tmp_project):
    legacy = tmp_project / "oryx.local.toml"
    legacy.write_text('[ide]\nkind = "vscode"\ndebugger = "lldb"\n', encoding="utf-8")
    result = forge("config", "init", "--debugger", "cppdbg")
    assert result.exit_code == 0, result.output
    assert "rename it to forge.local.toml" in result.output
    assert (tmp_project / "forge.local.toml").read_text(encoding="utf-8") == '[editor]\nkind = "vscode"\ndebugger = "cppdbg"\n'
    assert legacy.read_text(encoding="utf-8") == '[ide]\nkind = "vscode"\ndebugger = "lldb"\n'


def test_config_init_vscode_writes_four_files(forge, tmp_project):
    result = forge("config", "init", "--ide", "vscode", "--no-remember")
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
    for group in ("build", "config", "deps", "docs", "python", "setup", "test"):
        assert group in result.output
    assert "vendor" not in result.output


def test_invalid_profile_is_a_configuration_error(forge):
    result = forge("--profile", "bogus", "build", "compile")
    assert result.exit_code == 1
    assert result.output.startswith("Configuration Error: command line: '--profile' is 'bogus'; expected one of 'debug', 'release', 'dist'")


def test_schema_error_is_a_configuration_error(forge, tmp_project):
    (tmp_project / "forge.toml").write_text('[project]\nname = "x"\n[build]\njob = 2\n', encoding="utf-8")
    result = forge("build", "compile")
    assert result.exit_code == 1
    assert "unknown key 'build.job' — did you mean 'jobs'?" in result.output


def test_runs_from_a_subdirectory(forge, tmp_project, monkeypatch):
    (tmp_project / "Oasis" / "src").mkdir(parents=True)
    monkeypatch.chdir(tmp_project / "Oasis" / "src")
    result = forge("--dry-run", "build", "clean")
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
    result = forge(*flags, "build", "configure")
    assert result.exit_code == 1
    assert message in result.output


def test_local_options_override_the_defaults(dry, premake, tmp_project):
    (tmp_project / "forge.local.toml").write_text("[options]\npython = false\n", encoding="utf-8")
    assert dry("build", "configure") == [f" would run: {premake} gmake --no-python --forge-export"]
    assert dry("--with", "python", "build", "configure") == [f" would run: {premake} gmake {PYTHON_OPTIONS} --forge-export"]


def test_hidden_aliases_stay_out_of_help(forge):
    output = forge("--help").output
    assert "--with" in output and "--without" in output
    assert "--no-python" not in output and "--sanitize" not in output
