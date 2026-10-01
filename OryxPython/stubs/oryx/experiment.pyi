"""
Describing, running and storing experiments.
"""
from __future__ import annotations
import collections.abc
import typing
__all__: list[str] = ['Experiment', 'ExperimentResult', 'Tournament']
class Experiment:
    """
    A set of matchups run for several repeats with seeds derived from the master seed; pure data until run().
    """
    @staticmethod
    def sweep(game: typing.Any, strategies: typing.Any, axes: dict, *, game_params: typing.Any = None, name: str = 'sweep', matches: typing.SupportsInt | typing.SupportsIndex = 100, repeats: typing.SupportsInt | typing.SupportsIndex = 1, seed: typing.SupportsInt | typing.SupportsIndex = 0) -> Experiment:
        """
        One matchup per combination of `axes`, keyed by 'game.<param>' or 'seats.<index>.<param>'.
        """
    def __init__(self, matchups: collections.abc.Sequence, *, name: str = 'experiment', matches: typing.SupportsInt | typing.SupportsIndex = 100, repeats: typing.SupportsInt | typing.SupportsIndex = 1, seed: typing.SupportsInt | typing.SupportsIndex = 0) -> None:
        """
        Each matchup is a dict with `game`, `strategies` (one entry per seat), and optionally `game_params` and `label`.
        """
    def __repr__(self) -> str:
        ...
    def run(self, progress: collections.abc.Callable[[int, int], typing.Any] | None = None, *, timestamp: bool = False, diagnostics: bool = False) -> ...:
        """
        Runs every trial; `progress(done, total)` is called after each, and Ctrl-C stops between trials. `diagnostics=True` adds what strategies publish (minimax/nodes) to each trial's metrics.
        """
    def validate(self) -> None:
        """
        Checks ids, parameters and capabilities without running anything.
        """
    @property
    def spec(self) -> dict:
        ...
class ExperimentResult:
    """
    The trials of a run with the spec and metadata that produced them.
    """
    @staticmethod
    def load(path: str | os.PathLike[str]) -> ExperimentResult:
        ...
    def __repr__(self) -> str:
        ...
    def _repr_html_(self) -> str:
        ...
    def compare(self, matchup_a: str, matchup_b: str, metric: str) -> dict:
        """
        Difference of a metric's mean across repeats between two matchups, with a 95% interval.
        """
    def cross_table(self) -> dict:
        """
        Scores of every strategy against every other (win = 1, draw = 0.5); two-seat matchups only.
        """
    def ratings(self) -> dict:
        """
        Bradley-Terry strengths on the Elo scale, centred on 0; two-seat matchups only.
        """
    def rerun(self, progress: collections.abc.Callable[[int, int], typing.Any] | None = None, *, timestamp: bool = False, diagnostics: bool = False) -> ExperimentResult:
        """
        Runs the stored spec again; the trials must match exactly, so pass the same `diagnostics`.
        """
    def same_trials(self, other: ExperimentResult) -> bool:
        """
        Whether both results hold identical trials, ignoring order and metadata.
        """
    def save(self, path: str | os.PathLike[str]) -> None:
        """
        Writes result.yaml and trials.csv into the directory `path`.
        """
    def series(self, matchup: str, metric: str) -> list[float]:
        """
        One value per repeat.
        """
    def summarize(self, matchup: str, metric: str) -> dict:
        """
        Mean, stddev and 95% CI half-width of a metric across repeats.
        """
    def summary(self) -> list:
        """
        Per matchup: totals, win rates with Wilson intervals, draw rate.
        """
    def to_dataframe(self) -> typing.Any:
        """
        Tidy rows (matchup, repeat, metric, value); needs pandas.
        """
    def to_dict(self) -> dict:
        ...
    @property
    def complete(self) -> bool:
        """
        False when the run was cancelled before every trial finished.
        """
    @property
    def metadata(self) -> dict:
        ...
    @property
    def spec(self) -> dict:
        ...
class Tournament(Experiment):
    """
    A round robin of a strategy pool: every pair plays, with both seatings when `rotate_seats` cancels first-player bias.
    """
    def __init__(self, game: typing.Any, strategies: collections.abc.Sequence, *, rotate_seats: bool = True, game_params: typing.Any = None, name: str = 'tournament', matches: typing.SupportsInt | typing.SupportsIndex = 100, repeats: typing.SupportsInt | typing.SupportsIndex = 1, seed: typing.SupportsInt | typing.SupportsIndex = 0) -> None:
        ...
