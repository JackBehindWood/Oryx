"""Scaffolding for adding a new vendored third-party library.

The vendored submodule directory (<project>/vendor/<lib>/) is always left as
a pristine, untouched upstream checkout — never written into. A compiled
(non-header-only) library instead gets its own generated build script at
<project>/vendor/premake/<lib>.lua — a sibling of <lib>/ within vendor/,
keeping everything vendor-related under one directory — referencing the
vendor sources by relative path.

Rather than being `include`d from the consuming project, that script becomes
its own top-level workspace project, `include`d from the root premake5.lua
under `group "Dependencies"` — mirroring Hazel's dependency convention. Its
include path is registered once in premake/dependencies.lua's `IncludeDir`
table, referenced as `%{IncludeDir.<lib>}` by every consuming project's
`premake5.lua`, instead of each repeating the raw path.

(See premake/vendor.lua's useVendorHeader() for the header-only equivalent —
header-only libs stay out of the Dependencies group, matching Hazel, since
they compile nothing — and build_system/vendor.py for the read side of the
<project>/vendor/<lib>/ convention, which excludes the premake/ subdir
itself from being treated as a vendored lib.)
"""

import re
from pathlib import Path

from build_system.config import PROJECT_ROOT
from build_system.utils import run_command

STATIC_LIB_TEMPLATE = """\
-- Vendored static library: {name}
-- Source: {project}/vendor/{name}/ (untouched upstream checkout — do not
-- add files there; edit this file instead).
project "{name}"
    kind "StaticLib"
    language "C++"
    staticruntime "off"
    warnings "Off"  -- third-party code; don't enforce our own warning level

    targetdir ("%{{wks.location}}/bin/" .. outputdir .. "/%{{prj.name}}")
    objdir ("%{{wks.location}}/bin-int/" .. outputdir .. "/%{{prj.name}}")

    files {{
        "{source_root}/**.h",
        "{source_root}/**.hpp",
        "{source_root}/**.c",
        "{source_root}/**.cpp",
    }}

    includedirs {{
        "{include_path}"
    }}{defines_block}
"""


def vendor_path(project: str, name: str) -> Path:
    return PROJECT_ROOT / project / "vendor" / name


def premake_script_path(project: str, name: str) -> Path:
    return PROJECT_ROOT / project / "vendor" / "premake" / f"{name}.lua"


def add_git_submodule(url: str, project: str, name: str) -> None:
    """Run `git submodule add`. A no-op error if the path is already a
    submodule (e.g. re-running after a partial failure)."""
    path = vendor_path(project, name)
    run_command(["git", "submodule", "add", url, str(path.relative_to(PROJECT_ROOT))], cwd=PROJECT_ROOT)


def _vendor_subpath(name: str, subdir: str | None) -> str:
    # Relative to vendor/premake/<lib>.lua, its sibling vendor/<lib>/ checkout
    # is one level up (out of premake/) then into <name>/.
    base = f"../{name}"
    return f"{base}/{subdir}" if subdir else base


def _defines_block(defines: list[str] | None) -> str:
    if not defines:
        return ""
    entries = "\n".join(f'        "{d}",' for d in defines)
    return f"\n\n    defines {{\n{entries}\n    }}"


def write_static_lib_script(
    project: str,
    name: str,
    include_subdir: str | None = None,
    source_subdir: str | None = None,
    defines: list[str] | None = None,
) -> Path:
    """Write <project>/vendor/premake/<name>.lua — never touches vendor/<name>/.
    `include_subdir`/`source_subdir` scope the includedirs/files globs to a
    subdirectory of vendor/<name>/ (e.g. spdlog: headers under `include/`,
    compiled sources under `src/` — vendoring the whole checkout would also
    glob its example/tests/bench sources). `defines` are written as a
    `defines {}` block, for libraries whose compiled-lib mode needs a
    preprocessor define (e.g. spdlog's SPDLOG_COMPILED_LIB)."""
    path = premake_script_path(project, name)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        STATIC_LIB_TEMPLATE.format(
            name=name,
            project=project,
            source_root=_vendor_subpath(name, source_subdir),
            include_path=_vendor_subpath(name, include_subdir),
            defines_block=_defines_block(defines),
        ),
        encoding="utf-8",
    )
    return path


def insert_header_only_usage(project: str, name: str, header_subdir: str | None) -> tuple[bool, str]:
    """Insert a useVendorHeader(...) call into <project>/premake5.lua, right
    after useOryxProjectDefaults(). Returns (edited, snippet): edited is
    False when the expected anchor line isn't found (e.g. the file was
    hand-edited since this CLI last wrote it) — the caller should print
    `snippet` for the user to paste manually instead."""
    call = f'useVendorHeader("{name}")' if not header_subdir else f'useVendorHeader("{name}", "{header_subdir}")'

    path = PROJECT_ROOT / project / "premake5.lua"
    text = path.read_text(encoding="utf-8")

    if call in text:
        return True, call

    anchor = "useOryxProjectDefaults()"
    if anchor not in text:
        return False, call

    text = text.replace(anchor, f"{anchor}\n\n    {call}", 1)
    path.write_text(text, encoding="utf-8")
    return True, call


def _insert_into_brace_list(text: str, block_name: str, entry: str) -> str | None:
    """Insert `entry` as the first item of an existing `<block_name> { ... }`
    block. Returns the updated text, or None if the block isn't found."""
    pattern = re.compile(rf"({re.escape(block_name)}\s*\{{\s*\n)")
    match = pattern.search(text)
    if not match:
        return None
    insertion = f'        "{entry}",\n'
    return text[: match.end()] + insertion + text[match.end() :]


def _insert_or_append_block(text: str, block_name: str, entries: list[str]) -> str:
    """Insert each entry as a leading item of an existing `<block_name> { ... }`
    block, or append a brand new block at the end of the file if none exists."""
    updated = text
    for entry in entries:
        result = _insert_into_brace_list(updated, block_name, entry)
        if result is None:
            lines = "\n".join(f'        "{e}",' for e in entries)
            return text.rstrip("\n") + f'\n\n    {block_name} {{\n{lines}\n    }}\n'
        updated = result
    return updated


ROOT_PREMAKE_PATH = PROJECT_ROOT / "premake5.lua"
DEPENDENCIES_LUA_PATH = PROJECT_ROOT / "premake" / "dependencies.lua"


def _add_include_dir_entry(name: str, path_expr: str) -> bool:
    """Insert `IncludeDir["<name>"] = "<path_expr>"` into
    premake/dependencies.lua, right after `IncludeDir = {}`. Returns False
    if that file/anchor isn't found (e.g. hand-edited since last written)."""
    if not DEPENDENCIES_LUA_PATH.exists():
        return False
    text = DEPENDENCIES_LUA_PATH.read_text(encoding="utf-8")
    entry = f'IncludeDir["{name}"] = "{path_expr}"'
    if entry in text:
        return True
    anchor = "IncludeDir = {}"
    if anchor not in text:
        return False
    text = text.replace(anchor, f"{anchor}\n{entry}", 1)
    DEPENDENCIES_LUA_PATH.write_text(text, encoding="utf-8")
    return True


def _add_dependency_group_include(include_path: str) -> bool:
    """Insert `include "<include_path>"` into the root premake5.lua's
    `group "Dependencies"` block, creating that group (right before
    `group "Core"`) if it doesn't exist yet. Returns False if neither anchor
    is found."""
    text = ROOT_PREMAKE_PATH.read_text(encoding="utf-8")
    include_line = f'include "{include_path}"'
    if include_line in text:
        return True

    group_anchor = 'group "Dependencies"\n'
    if group_anchor in text:
        text = text.replace(group_anchor, f"{group_anchor}    {include_line}\n", 1)
        ROOT_PREMAKE_PATH.write_text(text, encoding="utf-8")
        return True

    core_anchor = 'group "Core"'
    if core_anchor not in text:
        return False
    new_group = f'group "Dependencies"\n    {include_line}\ngroup ""\n\n'
    text = text.replace(core_anchor, f"{new_group}{core_anchor}", 1)
    ROOT_PREMAKE_PATH.write_text(text, encoding="utf-8")
    return True


def insert_static_lib_wiring(
    project: str,
    name: str,
    include_subdir: str | None = None,
    defines: list[str] | None = None,
) -> tuple[bool, list[str]]:
    """Wire a scaffolded <project>/vendor/premake/<name>.lua in as its own
    top-level workspace project (mirrors Hazel): `include` it from the root
    premake5.lua's `group "Dependencies"` (created if needed) and register
    its include path once as `IncludeDir["<name>"]` in
    premake/dependencies.lua. Then, in the *consuming* <project>/premake5.lua,
    add "%{IncludeDir.<name>}" to includedirs, "<name>" to links, and (if
    given) each define to a defines block — links/defines blocks are created
    if none exist. Best-effort per file: any piece that can't be safely
    located is skipped, with its snippet returned for the caller to paste in
    by hand instead."""
    include_path = f"{project}/vendor/premake/{name}.lua"
    includedir_expr = f"%{{_MAIN_SCRIPT_DIR}}/{project}/vendor/{name}"
    if include_subdir:
        includedir_expr += f"/{include_subdir}"

    snippets = [
        f'include "{include_path}"  # in the root premake5.lua\'s group "Dependencies"',
        f'IncludeDir["{name}"] = "{includedir_expr}"  # in premake/dependencies.lua',
        f'"%{{IncludeDir.{name}}}"  # add to {project}/premake5.lua\'s includedirs {{ ... }} block',
        f'"{name}"  # add to {project}/premake5.lua\'s links {{ ... }} block (create one if none exists)',
    ]
    snippets += [
        f'"{d}"  # add to {project}/premake5.lua\'s defines {{ ... }} block (create one if none exists)'
        for d in defines or []
    ]

    root_ok = _add_dependency_group_include(include_path)
    includedir_ok = _add_include_dir_entry(name, includedir_expr)
    if not (root_ok and includedir_ok):
        return False, snippets

    path = PROJECT_ROOT / project / "premake5.lua"
    text = path.read_text(encoding="utf-8")

    with_includedirs = _insert_into_brace_list(text, "includedirs", f"%{{IncludeDir.{name}}}")
    if with_includedirs is None:
        return False, snippets
    text = with_includedirs

    text = _insert_or_append_block(text, "links", [name])
    if defines:
        text = _insert_or_append_block(text, "defines", defines)

    path.write_text(text, encoding="utf-8")
    return True, snippets
