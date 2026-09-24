import platform
import re
from pathlib import Path


def _detect_arch() -> str:
    machine = platform.machine().lower()
    return "ARM64" if machine in ("arm64", "aarch64") else "x64"


def make_config_token(profile: str) -> str:
    return f"{profile}_{_detect_arch().lower()}"


def outputdir(build_dir: Path, profile: str, make_project: str) -> str:
    make_file = build_dir / f"{make_project}.make"
    if make_file.is_file():
        match = re.search(
            rf"ifeq \(\$\(config\),{re.escape(make_config_token(profile))}\)[\s\S]*?\nTARGETDIR = bin/([^/]+)/",
            make_file.read_text(encoding="utf-8"),
        )
        if match:
            return match.group(1)
    system = {"Darwin": "macosx", "Linux": "linux", "Windows": "windows"}.get(platform.system(), platform.system().lower())
    return f"{profile.capitalize()}-{system}-{_detect_arch()}"


def target_path(build_dir: Path, profile: str, make_project: str, target: str) -> Path:
    return build_dir / "bin" / outputdir(build_dir, profile, make_project) / target / target
