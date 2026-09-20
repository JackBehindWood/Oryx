"""``oryx.assertions.check(condition, message)`` logs a failed check, then raises OryxAssertionError."""

from _oryx import assertions as _native

check = _native.check

__all__ = ["check"]
