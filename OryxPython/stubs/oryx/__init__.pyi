"""
Oryx's scripting API: games, strategies, matches, simulation and debugging.
"""
from __future__ import annotations
from oryx.errors import OryxAssertionError
from oryx.errors import OryxError
from oryx.errors import ParamError
from oryx.errors import ScriptError
from oryx.game import ActionFeatures
from oryx.game import Context
from oryx.game import Game
from oryx.game import GameHandle
from oryx.game import State
from oryx.game import StateHandle
from oryx.game import Strategy
from oryx.game import StrategyHandle
from oryx.random import Random
from oryx.registry import describe_game
from oryx.registry import describe_strategy
from oryx.registry import list_games
from oryx.registry import list_strategies
from oryx.registry import make_game
from oryx.registry import make_strategy
from oryx.registry import register_game
from oryx.registry import register_strategy
from oryx.results import BatchResult
from oryx.simulation import BatchRunner
from oryx.simulation import Match
from oryx.simulation import simulate
from . import benchmark
from . import debug
from . import errors
from . import game
from . import math
from . import random
from . import registry
from . import results
from . import simulation
__all__: list[str] = ['ActionFeatures', 'BatchResult', 'BatchRunner', 'Context', 'Game', 'GameHandle', 'Match', 'OryxAssertionError', 'OryxError', 'ParamError', 'Random', 'ScriptError', 'State', 'StateHandle', 'Strategy', 'StrategyHandle', 'benchmark', 'debug', 'describe_game', 'describe_strategy', 'errors', 'game', 'init', 'is_embedded_host', 'list_games', 'list_strategies', 'make_game', 'make_strategy', 'math', 'random', 'register_game', 'register_strategy', 'registry', 'results', 'simulate', 'simulation']
def init() -> None:
    """
    Initialises Oryx for a standalone Python process (idempotent) and installs the throwing assertion handler, so a C++ assert reached from Python raises OryxAssertionError instead of logging and trapping. Raises if called inside an embedding host such as Oasis.
    """
def is_embedded_host() -> bool:
    """
    True when running inside an embedding host such as Oasis; false for a standalone research-host process.
    """
