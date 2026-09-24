import os
import subprocess
import sys
from pathlib import Path

import oryx
import pytest

_SCRIPTS_DIR = Path(__file__).resolve().parents[2] / "Oasis" / "scripts"


def _run(code, **kwargs):
    return subprocess.run([sys.executable, "-c", code], **kwargs)


def test_import_oryx_works_standalone():
    assert _run("import oryx; oryx.init()").returncode == 0


def test_the_context_cannot_be_recreated_once_the_atexit_teardown_ran():
    code = """
import atexit, os


def late():
    import oryx
    try:
        class Late(oryx.Strategy, id="late"):
            def decide(self, context):
                return 0
    except oryx.OryxError as e:
        os._exit(0 if "shutting down" in str(e) else 4)
    os._exit(3)


atexit.register(late)
import oryx

oryx.init()


class Early(oryx.Strategy, id="early"):
    def decide(self, context):
        return 0
"""
    assert _run(code).returncode == 0


def test_benchmark_memory_true_counts_oryxs_own_allocations_and_balances_them():
    assert oryx.benchmark.benchmark("nim", ["random", "random"], games=5).memory is None
    m = oryx.benchmark.benchmark("nim", ["random", "random"], games=20, memory=True).memory
    assert m.allocation_count > 0
    assert m.allocation_count == m.deallocation_count
    assert m.live_bytes == 0


def test_init_loads_the_scripts_of_the_nearest_oryx_yaml_an_explicit_settings_file_or_nothing(tmp_path):
    (tmp_path / "project" / "scripts").mkdir(parents=True)
    (tmp_path / "project" / "scripts" / "pile.py").write_text(
        "import oryx\n\n\nclass Pile(oryx.Game, id='pile'):\n"
        "    num_players = 2\n\n    def new_initial_state(self):\n        return None\n"
    )
    (tmp_path / "project" / "oryx.yaml").write_text("scripting:\n  roots:\n    - scripts\n")
    nested = tmp_path / "project" / "notebooks"
    nested.mkdir()
    elsewhere = tmp_path / "elsewhere"
    elsewhere.mkdir()
    settings = tmp_path / "project" / "oryx.yaml"

    assert (
        _run("import sys, oryx; oryx.init(); sys.exit(0 if 'pile' in oryx.list_games() else 1)", cwd=nested).returncode
        == 0
    )
    assert (
        _run("import sys, oryx; oryx.init(); sys.exit(0 if oryx.list_games() == [] else 1)", cwd=elsewhere).returncode
        == 0
    )
    assert (
        _run(
            f"import sys, oryx; oryx.init({str(settings)!r}); sys.exit(0 if 'pile' in oryx.list_games() else 1)",
            cwd=elsewhere,
        ).returncode
        == 0
    )
    assert (
        _run(
            "import sys, oryx\ntry:\n    oryx.init('/no/such/oryx.yaml')\nexcept oryx.SettingsError:\n    sys.exit(0)\nsys.exit(1)",
            cwd=elsewhere,
        ).returncode
        == 0
    )


def test_a_scripted_game_and_simulate_exit_cleanly_under_the_debug_allocator():
    code = (
        f"import sys; sys.path.insert(0, {str(_SCRIPTS_DIR)!r})\n"
        "import oryx, nim\n"
        "oryx.init()\n"
        "result = oryx.simulate('nim', ['random', 'random'], games=20)\n"
        "sys.exit(0 if result.matches == 20 else 1)\n"
    )
    assert _run(code, env={**os.environ, "PYTHONMALLOC": "debug"}).returncode == 0


def test_assertion_hook_raises_instead_of_trapping():
    with pytest.raises(oryx.errors.OryxAssertionError):
        oryx.debug.check(False, "x")


def test_interruptible_batch_raises_when_the_interpreter_is_interrupted_mid_batch():
    # Bare `import oryx` registers no C++ game, so this interrupts a scripted one: the
    # interrupt must pass through its callback untouched.
    code = (
        f"import sys; sys.path.insert(0, {str(_SCRIPTS_DIR)!r})\n"
        "import oryx, nim, signal, threading, _thread\n"
        "oryx.init()\n"
        "signal.signal(signal.SIGINT, signal.default_int_handler)\n"
        "threading.Timer(0.05, _thread.interrupt_main).start()\n"
        "try:\n"
        "    oryx.simulate('nim', ['random', 'random'], games=2_000_000)\n"
        "except KeyboardInterrupt:\n"
        "    sys.exit(0)\n"
        "sys.exit(1)\n"
    )
    assert _run(code).returncode == 0
