import subprocess

from ..config import BUILD_DIR, BuildConfig
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
