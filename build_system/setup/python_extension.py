"""Writes the .pth file that makes `import oryx` work from the active venv with no path setup."""

from pathlib import Path
import sysconfig


def install_extension_pth(extension_dir: Path) -> None:
    site_packages = Path(sysconfig.get_path("purelib"))
    pth = site_packages / "oryx_research_host.pth"
    pth.write_text(str(extension_dir.resolve()) + "\n", encoding="utf-8")
