import os
import sys
from pathlib import Path

import pytest

# Bare `import oryx` registers no C++ game (those live in Oasis, not the research-host
# extension), so tests use the scripts below for game/strategy-shaped assertions. Inserted
# at collection time, before any fixture runs, so test modules can `import nim`/`import
# monte_carlo` at module scope too.
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Oasis" / "scripts"))


@pytest.fixture(scope="session", autouse=True)
def _oryx_extension():
    try:
        import oryx
    except ImportError as error:
        if os.environ.get("ORYX_REQUIRE_EXTENSION"):
            raise
        pytest.skip(f"the oryx research-host extension isn't importable: {error}")
        return
    import monte_carlo  # noqa: F401
    import nim  # noqa: F401
    oryx.init()
