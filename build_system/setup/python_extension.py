"""Writes the .pth file that makes `import oryx` work from the active venv with no path setup."""

from pathlib import Path
import sysconfig


def _pth_path() -> Path:
    return Path(sysconfig.get_path("purelib")) / "oryx_research_host.pth"


def install_extension_pth(extension_dir: Path) -> None:
    _pth_path().write_text(str(extension_dir.resolve()) + "\n", encoding="utf-8")


def remove_extension_pth() -> bool:
    """Returns whether a .pth file was there to remove."""
    pth = _pth_path()
    if not pth.exists():
        return False
    pth.unlink()
    return True
