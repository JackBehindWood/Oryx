import os
from pathlib import Path

from build_system.config import BuildConfig
from build_system.setup.generators import build_compile_command


def test_gmake_builds_in_parallel():
    command = build_compile_command(BuildConfig(), Path("build"))
    assert f"-j{os.cpu_count()}" in command
