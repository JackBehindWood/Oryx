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
        (["--sanitize"], f"{PYTHON_OPTIONS} --sanitize"),
        (["--no-python", "--sanitize"], "--no-python --sanitize"),
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
        (["--game", "tictactoe", "--opponent", "human"], " --game=tictactoe --opponent=human"),
        (["--simulate", "random,first-legal,100", "--benchmark"], " --simulate=random,first-legal,100 --benchmark"),
    ],
)
def test_run(dry, tmp_project, args, passed):
    oasis = tmp_project / "build" / "bin" / "Debug-linux-x86_64" / "Oasis" / "Oasis"
    assert dry("build", "run", *args) == [f" would run: {oasis}{passed}"]


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
    for group in ("build", "config", "docs", "python", "setup", "test", "vendor"):
        assert group in result.output


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
