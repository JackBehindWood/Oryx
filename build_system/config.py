from dataclasses import dataclass
from pathlib import Path
import json
import platform

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BIN_DIR = PROJECT_ROOT / "bin"
CONFIG_FILE = PROJECT_ROOT / "build_config.json"

VALID_CONFIGS = {"debug", "release", "dist"}

@dataclass
class BuildConfig:
    project_name: str = "Oryx"
    build_generator: str = "gmake2"
    test_executable: str = "Tests"
    config: str = "debug"  # debug, release, or dist

    def __post_init__(self):
        """Validate configuration settings."""
        self.config = self.config.lower()
        if self.config not in VALID_CONFIGS:
            raise ValueError(f"Invalid config '{self.config}'. Must be one of: {', '.join(VALID_CONFIGS)}")

    @property
    def outputdir(self) -> str:
        """Reconstruct the premake outputdir string (%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture})."""
        # Map build mode to Premake's exact casing
        cfg_name = self.config.capitalize()  # "Debug", "Release", "Dist"
        
        # Determine OS name used by Premake os.host()
        system_map = {"Darwin": "macosx", "Linux": "linux", "Windows": "windows"}
        system = system_map.get(platform.system(), platform.system().lower())

        # Determine Architecture used by Premake
        machine = platform.machine().lower()
        arch = "ARM64" if machine in ("arm64", "aarch64") else "x64"

        return f"{cfg_name}-{system}-{arch}"

    @property
    def binary_path(self) -> Path:
        """Get the full output directory path for built binaries."""
        return BIN_DIR / self.outputdir

    @classmethod
    def init(cls) -> "BuildConfig":
        """Initialize and create a default build_config.json if it does not exist."""
        if CONFIG_FILE.exists():
            print(f"⚠️ Configuration file already exists at: {CONFIG_FILE}")
            return cls.load()

        default_config = cls()
        default_config.save()
        print(f"✓ Created default build configuration at: {CONFIG_FILE}")
        return default_config

    @classmethod
    def load(cls) -> "BuildConfig":
        """Load configuration from JSON or return defaults."""
        if not CONFIG_FILE.exists():
            return cls()
        with CONFIG_FILE.open("r", encoding="utf-8") as file:
            data = json.load(file)
            return cls(**data)

    def save(self):
        """Save configuration to JSON file."""
        with CONFIG_FILE.open("w", encoding="utf-8") as file:
            json.dump(self.__dict__, file, indent=2)
            file.write("\n")