from pathlib import Path

from ..config import BuildConfig


def _gmake_compile_command(cfg: BuildConfig, makefile_dir: Path) -> list[str]:
    return ["make", "-C", str(makefile_dir), f"config={cfg.make_config_token}"]


# Maps a Premake generator name to a function that builds the shell command
# used to compile a project configured with that generator. Add an entry here
# to support a new generator — compile_project() itself stays unchanged.
COMPILE_COMMAND_BUILDERS = {
    "gmake": _gmake_compile_command,
}


def build_compile_command(cfg: BuildConfig, makefile_dir: Path) -> list[str]:
    """Build the shell command that compiles the project for cfg.build_generator."""
    builder = COMPILE_COMMAND_BUILDERS.get(cfg.build_generator)
    if builder is None:
        raise ValueError(f"Unsupported generator: {cfg.build_generator}")
    return builder(cfg, makefile_dir)
