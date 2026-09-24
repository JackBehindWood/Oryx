from pathlib import Path

from build_system.config import RunContext

from .c_cpp_properties import write_c_cpp_properties
from .launch import write_launch
from .settings import write_settings
from .tasks import write_tasks

__all__ = ["write_all", "write_tasks", "write_settings", "write_c_cpp_properties", "write_launch"]


def write_all(run: RunContext, debugger: str = "lldb") -> list[Path]:
    """Generate/merge every .vscode file this CLI knows how to produce."""
    return [
        write_tasks(run),
        write_settings(run),
        write_c_cpp_properties(run),
        write_launch(run, debugger=debugger),
    ]
