import re
import subprocess
from pathlib import Path

from ..utils import remove_directory, run_command
from ..workspace import Workspace


def prune_stale_object_dirs(workspace: Workspace, profile: str, build_dir: Path) -> list[str]:
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
    config_token = workspace.token(profile)

    for project in sorted(workspace.projects):
        make_file = build_dir / f"{project}.make"
        if not make_file.is_file():
            continue
        try:
            run_command(["make", "-n", "-f", make_file.name, f"config={config_token}"], cwd=build_dir)
        except (subprocess.CalledProcessError, FileNotFoundError) as error:
            if not _names_a_stale_source(getattr(error, "stderr", None) or "", build_dir):
                continue

            remove_directory(workspace.config(project, profile).objdir)
            cleaned.append(project)

    return cleaned


_MISSING_TARGET = re.compile(r"No rule to make target [`'\"]?([^'`\"]+)['`\"]?")


def _names_a_stale_source(stderr: str, build_dir: Path) -> bool:
    # A missing library another project builds (e.g. after its target was cleared) is not a stale .d entry.
    match = _MISSING_TARGET.search(stderr)
    return bool(match) and not (build_dir / match.group(1)).resolve().is_relative_to((build_dir / "bin").resolve())


def wipe_outputs_if_options_changed(build_dir: Path, previous_hash: str | None, current_hash: str) -> bool:
    """Wipe build/bin and build/bin-int when the Premake flags differ from the last configure.

    Premake's Makefiles don't rebuild an object when only its defines or buildoptions change, and
    `ar` keeps archive members from a previous build (also, --sanitize's own PCH is incompatible
    with a non-sanitized one, or vice versa), so toggling an option or moving to another
    interpreter would otherwise leave stale objects and outputs from the old flags. With no
    record of the previous flags, existing outputs are wiped too. Returns whether it wiped.
    """
    (build_dir / ".python-options").unlink(missing_ok=True)
    changed = bool(previous_hash) and previous_hash != current_hash
    if not (changed or (not previous_hash and (build_dir / "bin").exists())):
        return False
    remove_directory(build_dir / "bin")
    remove_directory(build_dir / "bin-int")
    return True
