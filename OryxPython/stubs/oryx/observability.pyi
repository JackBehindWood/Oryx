"""
What a strategy reports about a decision.
"""
from __future__ import annotations
import collections.abc
import typing
__all__: list[str] = ['Decision', 'Observer']
class Decision:
    """
    One decision of a traced match: who chose what, the strategy's scores and its diagnostics.
    """
    def __repr__(self) -> str:
        ...
    def to_dict(self) -> dict:
        ...
    @property
    def chosen(self) -> int:
        ...
    @property
    def chosen_label(self) -> str:
        ...
    @property
    def extra(self) -> dict:
        """
        Namespaced diagnostics such as minimax/nodes.
        """
    @property
    def player(self) -> int:
        ...
    @property
    def ply(self) -> int:
        ...
    @property
    def scores(self) -> list:
        """
        Per action: action, label, probability and value (None when the strategy gave none).
        """
    @property
    def tree(self) -> list:
        """
        Flat search-tree nodes (parent, action, visits, value); empty unless the strategy builds one.
        """
class Observer:
    """
    Where decide() publishes its Decision; only valid until decide() returns.
    """
    def publish(self, chosen: typing.SupportsInt | typing.SupportsIndex, *, probabilities: collections.abc.Mapping[typing.SupportsInt | typing.SupportsIndex, typing.SupportsFloat | typing.SupportsIndex] = {}, values: collections.abc.Mapping[typing.SupportsInt | typing.SupportsIndex, typing.SupportsFloat | typing.SupportsIndex] = {}, extra: collections.abc.Mapping[str, typing.SupportsFloat | typing.SupportsIndex] = {}) -> None:
        """
        Reports the action chosen with optional per-action probabilities and values and namespaced `extra` diagnostics ("mystrategy/nodes").
        """
