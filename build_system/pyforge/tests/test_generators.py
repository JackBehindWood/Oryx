import os
from pathlib import Path

import pytest

from pyforge.setup.generators import build_compile_command


def test_gmake_builds_in_parallel_on_all_cores_by_default():
    command = build_compile_command("gmake", "debug_x64", Path("build"))
    assert command == ["make", "-C", "build", f"-j{os.cpu_count()}", "config=debug_x64"]


def test_gmake_honours_explicit_jobs():
    assert "-j3" in build_compile_command("gmake", "debug_x64", Path("build"), jobs=3)


def test_unknown_generator():
    with pytest.raises(ValueError, match="Unsupported generator: ninja"):
        build_compile_command("ninja", "debug_x64", Path("build"))
