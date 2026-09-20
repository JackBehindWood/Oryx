"""Logging through Oryx's client logger, plus a bridge from the standard ``logging`` module."""

import logging

from _oryx import log as _native

trace = _native.trace
info = _native.info
warn = _native.warn
error = _native.error
critical = _native.critical


def _function_for(level):
    if level >= logging.CRITICAL:
        return critical
    if level >= logging.ERROR:
        return error
    if level >= logging.WARNING:
        return warn
    if level >= logging.INFO:
        return info
    return trace


class Handler(logging.Handler):
    """Forwards ``logging`` records to Oryx's client logger."""

    def __init__(self, level=logging.NOTSET):
        super().__init__(level)
        self.setFormatter(logging.Formatter("%(name)s: %(message)s"))

    def emit(self, record):
        try:
            _function_for(record.levelno)(self.format(record))
        except Exception:
            self.handleError(record)


def install(logger=None, level=logging.NOTSET):
    """Attach a Handler to ``logger`` (the root logger by default) and return it."""
    handler = Handler(level)
    (logger or logging.getLogger()).addHandler(handler)
    return handler


__all__ = ["Handler", "critical", "error", "info", "install", "trace", "warn"]
