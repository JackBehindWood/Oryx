import os
import sys
from pathlib import Path

import pytest


@pytest.fixture(scope="session", autouse=True)
def _oryx_extension():
    try:
        import oryx
    except ImportError as error:
        if os.environ.get("ORYX_REQUIRE_EXTENSION"):
            raise
        pytest.skip(f"the oryx research-host extension isn't importable: {error}")
        return
    # Bare `import oryx` registers no C++ game (those live in Oasis, not the research-host
    # extension), so tests use this scripted game for game-shaped assertions.
    sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "Oasis" / "scripts"))
    import nim  # noqa: F401
    oryx.init()
