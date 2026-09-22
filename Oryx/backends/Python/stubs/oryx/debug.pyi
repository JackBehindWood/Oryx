"""
Logging to Oryx's client logger and checks that raise OryxAssertionError instead of trapping the process.
"""
from __future__ import annotations
import logging
import typing
__all__: list[str] = ['Handler', 'check', 'critical', 'error', 'info', 'install', 'trace', 'warn']
class Handler(logging.Handler):
    """
    Forwards ``logging`` records to Oryx's client logger.
    """
    def emit(self, record: typing.Any) -> None:
        ...
    def format(self, record: typing.Any) -> typing.Any:
        ...
def check(condition: bool, message: str = '') -> None:
    ...
def critical(message: str) -> None:
    ...
def error(message: str) -> None:
    ...
def info(message: str) -> None:
    ...
def install(logger: typing.Any = None, level: typing.SupportsInt | typing.SupportsIndex = 0) -> typing.Any:
    """
    Attach a Handler to `logger` (the root logger by default) and return it.
    """
def trace(message: str) -> None:
    ...
def warn(message: str) -> None:
    ...
