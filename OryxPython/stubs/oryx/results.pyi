"""
The totals of a batch of matches.
"""
from __future__ import annotations
import typing
__all__: list[str] = ['BatchResult']
class BatchResult:
    """
    Totals from a batch of matches, with derived rates and, for simulate(), what was run.
    """
    def __repr__(self) -> str:
        ...
    def _repr_html_(self) -> str:
        ...
    def to_dataframe(self) -> typing.Any:
        """
        One row per player; needs pandas.
        """
    def to_dict(self) -> dict:
        ...
    def to_numpy(self) -> dict:
        """
        The counts and rates as numpy arrays; needs numpy.
        """
    @property
    def decisions(self) -> int:
        ...
    @property
    def draw_rate(self) -> float:
        ...
    @property
    def draws(self) -> int:
        ...
    @property
    def matches(self) -> int:
        ...
    @property
    def mean_rewards(self) -> list[float]:
        ...
    @property
    def metadata(self) -> dict[str, typing.Any] | None:
        """
        game, strategies, games, seed and oryx_version of a simulate() run; None for a BatchRunner.
        """
    @property
    def rewards(self) -> list[float]:
        ...
    @property
    def win_rates(self) -> list[float]:
        ...
    @property
    def wins(self) -> list[int]:
        ...
