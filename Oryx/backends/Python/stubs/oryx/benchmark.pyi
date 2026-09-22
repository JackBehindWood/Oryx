"""
Timing and memory measurements of simulations.
"""
from __future__ import annotations
import collections.abc
import oryx.results
import typing
__all__: list[str] = ['BenchmarkResult', 'MemoryStats', 'Timer', 'benchmark']
class BenchmarkResult:
    """
    The outcome of a benchmarked batch with its wall-clock time and throughput.
    """
    def __repr__(self) -> str:
        ...
    @property
    def decisions_per_second(self) -> float:
        ...
    @property
    def elapsed_seconds(self) -> float:
        ...
    @property
    def matches_per_second(self) -> float:
        ...
    @property
    def memory(self) -> typing.Any:
        """
        MemoryStats when benchmark(..., memory=True), else None.
        """
    @property
    def outcome(self) -> oryx.results.BatchResult:
        ...
class MemoryStats:
    """
    C++ heap allocations made by Oryx during a run; Python's own allocator is not counted.
    """
    def __repr__(self) -> str:
        ...
    @property
    def allocation_count(self) -> int:
        ...
    @property
    def bytes_allocated(self) -> int:
        ...
    @property
    def bytes_freed(self) -> int:
        ...
    @property
    def deallocation_count(self) -> int:
        ...
    @property
    def live_bytes(self) -> int:
        ...
    @property
    def peak_live_bytes(self) -> int:
        ...
class Timer:
    """
    Wall-clock timer; also a context manager. Read elapsed_seconds after stop().
    """
    def __enter__(self) -> Timer:
        ...
    def __exit__(self, *args) -> None:
        ...
    def __init__(self) -> None:
        ...
    def start(self) -> None:
        ...
    def stop(self) -> None:
        ...
    @property
    def elapsed_seconds(self) -> float:
        ...
def benchmark(game: typing.Any, strategies: collections.abc.Sequence, games: typing.SupportsInt | typing.SupportsIndex = 1000, seed: typing.Any = None, memory: bool = False) -> BenchmarkResult:
    """
    Plays `games` matches like simulate() and reports the time taken; memory=True also counts C++ allocations.
    """
