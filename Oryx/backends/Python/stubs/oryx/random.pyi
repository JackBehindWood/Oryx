"""
Random number generation.
"""
from __future__ import annotations
import typing
__all__: list[str] = ['Random']
class Random:
    """
    Uniform random number generator; seeded from the clock unless given a seed.
    """
    @typing.overload
    def __init__(self) -> None:
        ...
    @typing.overload
    def __init__(self, seed: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
    def get_bool(self, p: typing.SupportsFloat | typing.SupportsIndex = 0.5) -> bool:
        ...
    def get_double(self, min: typing.SupportsFloat | typing.SupportsIndex = 0.0, max: typing.SupportsFloat | typing.SupportsIndex = 1.0) -> float:
        ...
    def get_int(self, min: typing.SupportsInt | typing.SupportsIndex = 0, max: typing.SupportsInt | typing.SupportsIndex = 9223372036854775807) -> int:
        ...
    def seed(self, seed: typing.SupportsInt | typing.SupportsIndex) -> None:
        ...
