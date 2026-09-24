"""Real end-to-end coverage: drives an actual (non-Oryx) Premake project through the real
`forge` CLI as a subprocess — real Premake download, real compiler, real binary — proving
pyforge is genuinely project-independent, not just working by accident against Oryx's own
layout. See tests/fixtures/hello/."""

import platform
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

FIXTURE = Path(__file__).parent / "fixtures" / "hello"

_MISSING_TOOLCHAIN = platform.system() == "Windows" or shutil.which("make") is None or not any(shutil.which(cc) for cc in ("cc", "gcc", "clang"))


@pytest.fixture
def hello_project(tmp_path) -> Path:
    dest = tmp_path / "hello"
    shutil.copytree(FIXTURE, dest)
    return dest


def _forge(cwd: Path, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run([sys.executable, "-m", "pyforge.main", *args], cwd=cwd, capture_output=True, text=True)


@pytest.mark.slow
@pytest.mark.skipif(_MISSING_TOOLCHAIN, reason="no make/cc toolchain on this machine (expected on the Windows CI leg)")
def test_hello_configures_compiles_and_runs_end_to_end(hello_project):
    configure = _forge(hello_project, "configure")
    assert configure.returncode == 0, configure.stdout + configure.stderr
    assert (hello_project / "build" / "forge" / "workspace.json").is_file()

    compile_ = _forge(hello_project, "compile")
    assert compile_.returncode == 0, compile_.stdout + compile_.stderr

    run = _forge(hello_project, "run")
    assert run.returncode == 0, run.stdout + run.stderr
    assert "hello from pyforge" in run.stdout

    clean = _forge(hello_project, "clean")
    assert clean.returncode == 0, clean.stdout + clean.stderr
    assert not (hello_project / "build").exists()
