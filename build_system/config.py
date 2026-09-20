from dataclasses import dataclass, field
from pathlib import Path
import platform
import re
import tomllib

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build"
BIN_DIR = BUILD_DIR / "bin"
SITE_DIR = PROJECT_ROOT / "site"
MKDOCS_CONFIG = PROJECT_ROOT / "mkdocs.yml"
DEFAULT_CONFIG_FILE = PROJECT_ROOT / "oryx.toml"
DEFAULT_LOCAL_CONFIG_FILE = PROJECT_ROOT / "oryx.local.toml"

VALID_PROFILES = {"debug", "release", "dist"}
VALID_IDE_KINDS = {"vscode", "visual_studio", "none"}
VALID_DEBUGGERS = {"lldb", "cppdbg"}


def _detect_arch() -> str:
    """Map the host machine to Premake's architecture naming ("ARM64"/"x64")."""
    machine = platform.machine().lower()
    return "ARM64" if machine in ("arm64", "aarch64") else "x64"


@dataclass
class ExecutableConfig:
    """Config for a single compiled target, e.g. [executables.oasis] in oryx.toml.

    Only `name` (the compiled binary's name) exists today; this is its own
    dataclass/table — rather than a plain string — specifically so more
    per-executable settings (args, cwd, env, ...) can be added later without
    another schema migration.
    """
    name: str


@dataclass
class TestSuiteConfig:
    """Config for the test suite binary, [test-suite] in oryx.toml.

    Kept separate from [executables.*] — the test suite is a first-class
    concept with its own `test` command, not just another app — so it can grow
    test-specific settings (framework, filters, ...) independently later.
    """
    name: str = "Tests"


# Default compiled targets. Additional executables (benchmark harnesses, demo
# apps, etc.) can be added as [executables.<name>] blocks in oryx.toml without
# any code changes here — a command just calls cfg.executable_path("<name>").
DEFAULT_EXECUTABLES = {
    "oasis": ExecutableConfig(name="Oasis"),
}

@dataclass
class BuildConfig:
    project_name: str = "Oryx"
    build_generator: str = "gmake"
    profile: str = "debug"  # debug, release, or dist
    test_suite: TestSuiteConfig = field(default_factory=lambda: TestSuiteConfig(name="Tests"))
    executables: dict[str, ExecutableConfig] = field(default_factory=lambda: dict(DEFAULT_EXECUTABLES))

    def __post_init__(self):
        """Validate configuration settings."""
        self.profile = self.profile.lower()
        if self.profile not in VALID_PROFILES:
            raise ValueError(f"Invalid profile '{self.profile}'. Must be one of: {', '.join(VALID_PROFILES)}")

    @property
    def outputdir(self) -> str:
        """The output-folder suffix Premake actually generated
        (%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}), read straight out of
        the generated <project_name>.make file when available.

        Premake's own architecture-token spelling for a custom-named platform
        isn't guaranteed to match a guessed platform.machine() mapping — e.g. the
        vendored premake5 binary emits "AARCH64" for the "ARM64" platform on
        Apple Silicon, not "ARM64". Reading it out of Premake's own output avoids
        that guess going stale; the guess below is only a fallback for before
        `build configure` has run.
        """
        from_make = self._outputdir_from_make()
        if from_make:
            return from_make

        cfg_name = self.profile.capitalize()  # "Debug", "Release", "Dist"

        system_map = {"Darwin": "macosx", "Linux": "linux", "Windows": "windows"}
        system = system_map.get(platform.system(), platform.system().lower())

        return f"{cfg_name}-{system}-{_detect_arch()}"

    def _outputdir_from_make(self) -> str | None:
        make_file = BUILD_DIR / f"{self.project_name}.make"
        if not make_file.is_file():
            return None

        match = re.search(
            rf"ifeq \(\$\(config\),{re.escape(self.make_config_token)}\)\s*\nTARGETDIR = bin/([^/]+)/",
            make_file.read_text(encoding="utf-8"),
        )
        return match.group(1) if match else None

    @property
    def make_config_token(self) -> str:
        """The `config=<token>` value Premake's generated gmake files expect,
        e.g. "debug_arm64" (confirmed against the `ifeq ($(config),...)` blocks
        in a generated `.make` file)."""
        return f"{self.profile}_{_detect_arch().lower()}"

    @property
    def binary_path(self) -> Path:
        """Get the full output directory path for built binaries."""
        return BIN_DIR / self.outputdir

    def _resolve_target_path(self, target: str) -> Path:
        # useOryxProjectDefaults() (premake/common.lua) always sets targetdir
        # to end in "/%{prj.name}", so the binary always lands nested as
        # <target>/<target> — regardless of generator, profile, or whether it
        # has been built yet. (Do not resolve this by checking nested.exists():
        # that made the path depend on build state, silently falling back to a
        # wrong flat path — e.g. in generated launch.json — for any profile
        # that hadn't been compiled yet.)
        return self.binary_path / target / target

    def executable_path(self, name: str) -> Path:
        """Full path to a named target's compiled binary (see [executables.<name>] in oryx.toml)."""
        entry = self.executables.get(name)
        if entry is None:
            available = ", ".join(self.executables) or "(none configured)"
            raise KeyError(f"No executable named '{name}' configured. Available: {available}")
        return self._resolve_target_path(entry.name)

    def test_suite_path(self) -> Path:
        """Full path to the compiled test suite binary (see [test-suite] in oryx.toml)."""
        return self._resolve_target_path(self.test_suite.name)

    @classmethod
    def init(cls, path: Path = DEFAULT_CONFIG_FILE) -> "BuildConfig":
        """Initialize and create a default oryx.toml if it does not exist."""
        if path.exists():
            print(f"⚠️ Configuration file already exists at: {path}")
            return cls.load(path)

        default_config = cls()
        default_config.save(path)
        print(f"✓ Created default build configuration at: {path}")
        return default_config

    @classmethod
    def load(cls, path: Path = DEFAULT_CONFIG_FILE) -> "BuildConfig":
        """Load configuration from a TOML file, or return defaults."""
        if not path.exists():
            return cls()

        with path.open("rb") as file:
            data = tomllib.load(file)

        project = data.get("project", {})
        build = data.get("build", {})

        # [test-suite] is optional — omitting it keeps the default name.
        test_suite_data = data.get("test-suite", {})
        test_suite = TestSuiteConfig(name=test_suite_data.get("name", "Tests"))

        # [executables] is optional — anything not declared keeps its default.
        # Each entry is itself a table ([executables.<name>]), not a bare
        # string, so it can carry more than just `name` in the future.
        executables = dict(DEFAULT_EXECUTABLES)
        for key, entry in data.get("executables", {}).items():
            default = executables.get(key)
            default_name = default.name if default else key
            executables[key] = ExecutableConfig(name=entry.get("name", default_name))

        return cls(
            project_name=project.get("name", cls.project_name),
            build_generator=build.get("generator", cls.build_generator),
            profile=build.get("profile", cls.profile),
            test_suite=test_suite,
            executables=executables,
        )

    def save(self, path: Path = DEFAULT_CONFIG_FILE):
        """Save configuration to a TOML file."""
        lines = [
            "[project]",
            f'name = "{self.project_name}"',
            "",
            "[build]",
            f'generator = "{self.build_generator}"',
            f'profile = "{self.profile}"',
            "",
            "[test-suite]",
            f'name = "{self.test_suite.name}"',
        ]
        for key, entry in self.executables.items():
            lines.append("")
            lines.append(f"[executables.{key}]")
            lines.append(f'name = "{entry.name}"')
        lines.append("")

        path.write_text("\n".join(lines), encoding="utf-8")


@dataclass
class RunContext:
    """Global CLI runtime state, shared as the Typer context object."""
    config: BuildConfig
    config_path: Path = DEFAULT_CONFIG_FILE
    verbose: bool = False
    dry_run: bool = False


@dataclass
class LocalConfig:
    """Per-developer preferences — deliberately separate from BuildConfig /
    oryx.toml (shared, git-committed). Backed by oryx.local.toml, which is
    gitignored, so an IDE choice never collides across contributors or gets
    committed by accident.
    """
    ide_kind: str = "none"
    debugger: str = "lldb"

    def __post_init__(self):
        if self.ide_kind not in VALID_IDE_KINDS:
            raise ValueError(f"Invalid [ide] kind '{self.ide_kind}'. Must be one of: {', '.join(VALID_IDE_KINDS)}")
        if self.debugger not in VALID_DEBUGGERS:
            raise ValueError(f"Invalid [ide] debugger '{self.debugger}'. Must be one of: {', '.join(VALID_DEBUGGERS)}")

    @classmethod
    def init(cls, path: Path = DEFAULT_LOCAL_CONFIG_FILE) -> "LocalConfig":
        """Initialize and create a default oryx.local.toml if it does not exist."""
        if path.exists():
            return cls.load(path)

        default_config = cls()
        default_config.save(path)
        return default_config

    @classmethod
    def load(cls, path: Path = DEFAULT_LOCAL_CONFIG_FILE) -> "LocalConfig":
        """Load per-developer preferences, or defaults if never saved."""
        if not path.exists():
            return cls()

        with path.open("rb") as file:
            data = tomllib.load(file)

        ide = data.get("ide", {})
        return cls(
            ide_kind=ide.get("kind", cls.ide_kind),
            debugger=ide.get("debugger", cls.debugger),
        )

    def save(self, path: Path = DEFAULT_LOCAL_CONFIG_FILE) -> None:
        """Save per-developer preferences to oryx.local.toml."""
        lines = [
            "[ide]",
            f'kind = "{self.ide_kind}"',
            f'debugger = "{self.debugger}"',
            "",
        ]
        path.write_text("\n".join(lines), encoding="utf-8")
