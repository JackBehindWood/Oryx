import os

import pytest


@pytest.fixture(scope="session", autouse=True)
def _oryx_extension():
    try:
        import oryx  # noqa: F401
    except ImportError as error:
        if os.environ.get("ORYX_REQUIRE_EXTENSION"):
            raise
        pytest.skip(f"the oryx research-host extension isn't importable: {error}")
