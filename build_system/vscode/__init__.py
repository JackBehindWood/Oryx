from pathlib import Path

from build_system.config import BuildConfig

from .c_cpp_properties import write_c_cpp_properties
from .launch import write_launch
from .settings import write_settings
from .tasks import write_tasks

__all__ = ["write_all", "write_tasks", "write_settings", "write_c_cpp_properties", "write_launch"]


def write_all(cfg: BuildConfig, root: Path, debugger: str = "lldb") -> list[Path]:
    """Generate/merge every .vscode file this CLI knows how to produce."""
    return [
        write_tasks(root),
        write_settings(root),
        write_c_cpp_properties(cfg, root),
        write_launch(cfg, root, debugger=debugger),
    ]
