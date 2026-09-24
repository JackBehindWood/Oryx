import subprocess
from pathlib import Path

from ..utils import remove_directory, run_command


def prune_stale_object_dirs(config_token: str, outputdir: str, build_dir: Path) -> list[str]:
    """Clear out any project's object/dependency cache left stale by a
    source file move/rename/delete since the last build.

    Premake regenerates build/<Project>.make from the current file glob on
    every `configure`, but never touches build/bin-int/ (the object+.d
    cache). Object files are named by source basename only, so a moved file
    that kept its name leaves its old .d — still listing the old path as a
    prerequisite — sitting next to the freshly generated rule for the same
    object. The generated Makefile's `-include $(OBJECTS:%.o=%.d)` then pulls
    that stale prerequisite in too, and GNU Make fails with "No rule to make
    target <old path>" before compilation even starts.

    Detect this the same way build_system/compile_commands.py already does
    — dry-run each generated Makefile and read Make's own verdict, rather
    than reimplementing its dependency-file logic — and if it reports that
    error, wipe just that project's object dir for the current profile so
    the next real build regenerates it cleanly.
    """
    cleaned = []

    for make_file in sorted(build_dir.glob("*.make")):
        project = make_file.stem
        try:
            run_command(["make", "-n", "-f", make_file.name, f"config={config_token}"], cwd=build_dir)
        except (subprocess.CalledProcessError, FileNotFoundError) as error:
            stderr = getattr(error, "stderr", None) or ""
            if "No rule to make target" not in stderr:
                continue

            object_dir = build_dir / "bin-int" / outputdir / project
            remove_directory(object_dir)
            cleaned.append(project)

    return cleaned


def clear_outputs_if_python_changed(premake_options: list[str], build_dir: Path) -> bool:
    """Wipe build/bin and build/bin-int when the Python/--sanitize options differ from the last configure.

    Premake's Makefiles don't rebuild an object when only its defines or buildoptions change, and
    `ar` keeps archive members from a previous build (also, --sanitize's own PCH is incompatible
    with a non-sanitized one, or vice versa), so toggling --no-python, --sanitize, or moving to
    another interpreter would otherwise leave stale objects and outputs from the old options.
    """
    options_file = build_dir / ".python-options"
    current = "\n".join(premake_options)
    previous = options_file.read_text(encoding="utf-8") if options_file.is_file() else None

    changed = previous is not None and previous != current
    if changed or (previous is None and (build_dir / "bin").exists()):
        remove_directory(build_dir / "bin")
        remove_directory(build_dir / "bin-int")

    options_file.write_text(current, encoding="utf-8")
    return changed


SOURCE_ROOTS = {"Oryx/src": "Oryx", "Oryx/backends": "Oryx", "OryxPython": "OryxPython", "Oasis": "Oasis", "tests": "Tests"}
SOURCE_SUFFIXES = {".cpp", ".c", ".mm"}
SKIPPED_DIRS = {"vendor", "build", "bin", "bin-int", ".git", "__pycache__"}


def _manifest_file(root: Path) -> Path:
    return root / "build" / ".sources"


def source_manifest(root: Path) -> list[str]:
    """Every compiled source file Premake globs into a project, as sorted repo-relative paths."""
    sources = []
    for source_root in SOURCE_ROOTS:
        base = root / source_root
        for path in base.rglob("*") if base.is_dir() else []:
            relative = path.relative_to(base)
            if path.suffix in SOURCE_SUFFIXES and not SKIPPED_DIRS.intersection(relative.parts[:-1]):
                sources.append(path.relative_to(root).as_posix())
    return sorted(sources)


def _recorded_sources(root: Path) -> list[str] | None:
    manifest = _manifest_file(root)
    if not manifest.is_file():
        return None
    return manifest.read_text(encoding="utf-8").splitlines()


def sources_changed(root: Path) -> bool:
    """True when a source file was added or removed since the last configure, so the generated Makefiles are stale."""
    return _recorded_sources(root) != source_manifest(root)


def clear_outputs_of_removed_sources(root: Path) -> list[str]:
    """Delete the binaries of every project that lost a source file since the last configure.

    Premake regenerates the object list, but `ar` keeps the archive member of a removed source
    (the link then fails on symbols that no longer exist) and an executable is not relinked just
    because it lost an object (a deleted test would keep running from the old binary). Removing
    the project's binaries makes Make archive or link them again from the current objects.
    """
    previous = _recorded_sources(root)
    if previous is None:
        return []

    removed = set(previous) - set(source_manifest(root))
    projects = sorted({project for source_root, project in SOURCE_ROOTS.items() for path in removed if path.startswith(source_root + "/")})
    for project in projects:
        for output_dir in (root / "build" / "bin").glob(f"*/{project}"):
            remove_directory(output_dir)
    return projects


def record_source_manifest(root: Path) -> None:
    manifest = _manifest_file(root)
    manifest.parent.mkdir(parents=True, exist_ok=True)
    manifest.write_text("\n".join(source_manifest(root)), encoding="utf-8")
