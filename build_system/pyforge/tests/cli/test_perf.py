import subprocess
import sys
import time


def test_help_does_not_import_questionary_or_prompt_toolkit():
    """The interactive menu (questionary) is a `pyforge[menu]` extra: `forge --help` must not
    pay for it, matching the "you only pay for what you use" motto."""
    result = subprocess.run(
        [sys.executable, "-X", "importtime", "-m", "pyforge.main", "--help"],
        capture_output=True,
        text=True,
        check=True,
    )
    imported = {line.rsplit("|", 1)[-1].strip() for line in result.stderr.splitlines() if line.startswith("import time:")}
    assert "questionary" not in imported
    assert "prompt_toolkit" not in imported


def test_help_does_not_import_urllib_request():
    """Premake's download machinery (urllib.request, rich.progress) is only paid for by
    `forge premake install/update`, not by every invocation."""
    result = subprocess.run(
        [sys.executable, "-X", "importtime", "-m", "pyforge.main", "--help"],
        capture_output=True,
        text=True,
        check=True,
    )
    imported = {line.rsplit("|", 1)[-1].strip() for line in result.stderr.splitlines() if line.startswith("import time:")}
    assert "urllib.request" not in imported
    assert "rich.progress" not in imported


def test_help_cold_start_budget():
    """Generous on purpose (CI runners are noisy) — this catches a regression into seconds,
    not micro-optimizes milliseconds; see docs/tooling.md's perf table for the measured baseline."""
    start = time.perf_counter()
    subprocess.run([sys.executable, "-m", "pyforge.main", "--help"], capture_output=True, text=True, check=True)
    elapsed = time.perf_counter() - start
    assert elapsed < 2.0, f"forge --help took {elapsed:.2f}s, expected well under 2s"
