import os
import platform
from pathlib import Path

import pytest

from build_system.oryx.python_env import PythonBuildInfo


@pytest.fixture
def linux_host(monkeypatch):
    monkeypatch.setattr(platform, "system", lambda: "Linux")
    monkeypatch.setattr(platform, "machine", lambda: "x86_64")
    monkeypatch.setattr(os, "cpu_count", lambda: 8)


@pytest.fixture
def fake_python(monkeypatch):
    from build_system.oryx import python_env

    info = PythonBuildInfo(
        include_dir=Path("/py/include/python3.11"),
        lib_dir=Path("/py/lib"),
        lib_name="python3.11",
        home=Path("/py"),
        site_packages=(Path("/venv/lib/python3.11/site-packages"),),
    )
    monkeypatch.setattr(python_env, "python_build_info", lambda: info)
    return info
