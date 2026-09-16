from pathlib import Path

from build_system.config import PROJECT_ROOT
from build_system.utils import load_json, write_json
from build_system.vendor import vendor_dirs

SETTINGS_FILE = PROJECT_ROOT / ".vscode" / "settings.json"

# Keys this CLI owns outright and overwrites on every run.
GENERATED_SETTINGS = {
    "C_Cpp.default.configurationProvider": "ms-vscode.cpptools",
    "C_Cpp.default.intelliSenseEngine": "Tag Parser",
    "C_Cpp.formatting": "clangFormat",
    "C_Cpp.clang_format_fallbackStyle": "LLVM",
    "editor.formatOnSave": True,
    "[cpp]": {
        "editor.defaultFormatter": "ms-vscode.cpptools",
        "editor.formatOnSave": False,
    },
}

# Sub-keys merged into search.exclude / files.exclude rather than replacing
# those objects outright, so user-added entries survive regeneration.
GENERATED_SEARCH_EXCLUDE_BASE = {
    "build": True,
    "bin": True,
    "bin-int": True,
    ".git": True,
    # Only the downloaded premake5 binary is ignored/hidden — premake/*.lua
    # (common.lua, vendor.lua) are tracked helper scripts, kept visible.
    "premake/bin": True,
}

GENERATED_FILES_EXCLUDE = {
    ".DS_Store": True,
}


def _vendor_search_excludes() -> dict[str, bool]:
    # Every <project>/vendor/<lib>/ discovered on disk (see
    # build_system/vendor.py), instead of a single hardcoded doctest path.
    return {d.relative_to(PROJECT_ROOT).as_posix(): True for d in vendor_dirs()}


def write_settings(path: Path = SETTINGS_FILE) -> Path:
    """Generate or merge .vscode/settings.json with this CLI's defaults.

    Only the keys above are ever written; any other setting already present
    in the file (unrelated preferences, extra excludes, etc.) is preserved.
    """
    existing = load_json(path, default={})

    existing.update(GENERATED_SETTINGS)
    existing.setdefault("search.exclude", {}).update({**GENERATED_SEARCH_EXCLUDE_BASE, **_vendor_search_excludes()})
    existing.setdefault("files.exclude", {}).update(GENERATED_FILES_EXCLUDE)

    return write_json(path, existing)
