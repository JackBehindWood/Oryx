"""
Oryx's scripting API: games, strategies, matches, simulation and debugging.
"""
from __future__ import annotations
from oryx.board import ConsoleBoard
from oryx.errors import IllegalActionError
from oryx.errors import NotInitialisedError
from oryx.errors import OryxAssertionError
from oryx.errors import OryxError
from oryx.errors import ParamError
from oryx.errors import ScriptError
from oryx.errors import SettingsError
from oryx.experiment import Experiment
from oryx.experiment import ExperimentResult
from oryx.experiment import Tournament
from oryx.game import ActionFeatures
from oryx.game import Context
from oryx.game import Game
from oryx.game import GameHandle
from oryx.game import State
from oryx.game import StateHandle
from oryx.game import Strategy
from oryx.game import StrategyHandle
from oryx.observability import Decision
from oryx.observability import Observer
from oryx.random import Random
from oryx.registry import describe_game
from oryx.registry import describe_strategy
from oryx.registry import list_games
from oryx.registry import list_strategies
from oryx.registry import make_game
from oryx.registry import make_strategy
from oryx.registry import register_console_board
from oryx.registry import register_game
from oryx.registry import register_strategy
from oryx.results import BatchResult
from oryx.simulation import BatchRunner
from oryx.simulation import Match
from oryx.simulation import simulate
import typing
from . import benchmark
from . import board
from . import debug
from . import errors
from . import experiment
from . import game
from . import math
from . import observability
from . import random
from . import registry
from . import results
from . import simulation
__all__: list[str] = ['ActionFeatures', 'BatchResult', 'BatchRunner', 'ConsoleBoard', 'Context', 'Decision', 'Experiment', 'ExperimentResult', 'Game', 'GameHandle', 'IllegalActionError', 'Match', 'NotInitialisedError', 'Observer', 'OryxAssertionError', 'OryxError', 'ParamError', 'Random', 'ScriptError', 'SettingsError', 'State', 'StateHandle', 'Strategy', 'StrategyHandle', 'Tournament', 'benchmark', 'board', 'debug', 'describe_game', 'describe_strategy', 'errors', 'experiment', 'game', 'init', 'list_games', 'list_strategies', 'make_game', 'make_strategy', 'math', 'observability', 'random', 'register_console_board', 'register_game', 'register_strategy', 'registry', 'results', 'simulate', 'simulation']
def init(settings: str | os.PathLike[str] | None = None) -> None:
    """
    Initialises Oryx for a standalone Python process and installs the throwing assertion handler, so a C++ assert reached from Python raises OryxAssertionError. Reads `settings` (else the nearest oryx.yaml above the working directory, if any) and imports the scripts under its `scripting.roots`; calling it again loads nothing new.
    """
__version__: str = '0.1.0'
