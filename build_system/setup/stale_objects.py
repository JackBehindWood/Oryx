import subprocess

from ..config import BUILD_DIR, PROJECT_ROOT, BuildConfig
from ..utils import remove_directory, run_command


def prune_stale_object_dirs(cfg: BuildConfig) -> list[str]:
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

    for make_file in sorted(BUILD_DIR.glob("*.make")):
        project = make_file.stem
        try:
            run_command(["make", "-n", "-f", make_file.name, f"config={cfg.make_config_token}"], cwd=BUILD_DIR)
        except (subprocess.CalledProcessError, FileNotFoundError) as error:
            stderr = getattr(error, "stderr", None) or ""
            if "No rule to make target" not in stderr:
                continue

            object_dir = BUILD_DIR / "bin-int" / cfg.outputdir / project
            remove_directory(object_dir)
            cleaned.append(project)

    return cleaned


PYTHON_OPTIONS_FILE = BUILD_DIR / ".python-options"


def clear_outputs_if_python_changed(premake_options: list[str]) -> bool:
    """Wipe build/bin and build/bin-int when the Python/--sanitize options differ from the last configure.

    Premake's Makefiles don't rebuild an object when only its defines or buildoptions change, and
    `ar` keeps archive members from a previous build (also, --sanitize's own PCH is incompatible
    with a non-sanitized one, or vice versa), so toggling --no-python, --sanitize, or moving to
    another interpreter would otherwise leave stale objects and outputs from the old options.
    """
    current = "\n".join(premake_options)
    previous = PYTHON_OPTIONS_FILE.read_text(encoding="utf-8") if PYTHON_OPTIONS_FILE.is_file() else None

    changed = previous is not None and previous != current
    if changed or (previous is None and (BUILD_DIR / "bin").exists()):
        remove_directory(BUILD_DIR / "bin")
        remove_directory(BUILD_DIR / "bin-int")

    PYTHON_OPTIONS_FILE.write_text(current, encoding="utf-8")
    return changed


SOURCE_MANIFEST_FILE = BUILD_DIR / ".sources"
SOURCE_ROOTS = {"Oryx/src": "Oryx", "Oryx/backends": "Oryx", "OryxPython": "OryxPython", "Oasis": "Oasis", "tests": "Tests"}
SOURCE_SUFFIXES = {".cpp", ".c", ".mm"}
SKIPPED_DIRS = {"vendor", "build", "bin", "bin-int", ".git", "__pycache__"}


def source_manifest() -> list[str]:
    """Every compiled source file Premake globs into a project, as sorted repo-relative paths."""
    sources = []
    for root in SOURCE_ROOTS:
        base = PROJECT_ROOT / root
        for path in base.rglob("*") if base.is_dir() else []:
            relative = path.relative_to(base)
            if path.suffix in SOURCE_SUFFIXES and not SKIPPED_DIRS.intersection(relative.parts[:-1]):
                sources.append(path.relative_to(PROJECT_ROOT).as_posix())
    return sorted(sources)


def _recorded_sources() -> list[str] | None:
    if not SOURCE_MANIFEST_FILE.is_file():
        return None
    return SOURCE_MANIFEST_FILE.read_text(encoding="utf-8").splitlines()


def sources_changed() -> bool:
    """True when a source file was added or removed since the last configure, so the generated Makefiles are stale."""
    return _recorded_sources() != source_manifest()


def clear_outputs_of_removed_sources() -> list[str]:
    """Delete the binaries of every project that lost a source file since the last configure.

    Premake regenerates the object list, but `ar` keeps the archive member of a removed source
    (the link then fails on symbols that no longer exist) and an executable is not relinked just
    because it lost an object (a deleted test would keep running from the old binary). Removing
    the project's binaries makes Make archive or link them again from the current objects.
    """
    previous = _recorded_sources()
    if previous is None:
        return []

    removed = set(previous) - set(source_manifest())
    projects = sorted({project for root, project in SOURCE_ROOTS.items() for path in removed if path.startswith(root + "/")})
    for project in projects:
        for output_dir in (BUILD_DIR / "bin").glob(f"*/{project}"):
            remove_directory(output_dir)
    return projects


def record_source_manifest() -> None:
    SOURCE_MANIFEST_FILE.parent.mkdir(parents=True, exist_ok=True)
    SOURCE_MANIFEST_FILE.write_text("\n".join(source_manifest()), encoding="utf-8")
