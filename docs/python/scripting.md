# Scripting in Oasis

A game or strategy written in Python runs in Oasis exactly like a C++ one: it registers under an id, shows up in the menu with its description and parameters, and plays against any C++ or Python counterpart. Oasis embeds a Python interpreter, loads your scripts at start-up and never needs a rebuild when a script changes.

Python is on by default. Build with `--no-python` to leave it out; Oasis then has only the C++ games and strategies.

## Try it

The repository ships two example scripts in `Oasis/scripts/` (`nim.py` and `monte_carlo.py`): Nim and a Monte Carlo strategy that plays both Nim and TicTacToe. `Oasis/oryx.yaml` points Oasis at that directory.

```bash
uv run forge build run                                          # menu: pick a game, then an opponent
uv run forge build run -- --game=nim --opponent=monte-carlo     # play Nim against the Python strategy
uv run forge build run -- --game=nim --simulate=monte-carlo,first-legal,20
uv run forge build run -- --simulate=monte-carlo,tictactoe/heuristic,20
```

The menu prints each entry's description and its parameters with their defaults, for example `nim - Players alternate ... [stones=21, max_take=3]`. An entry with a required parameter (a typed field with no default) is left out of the menus, because Oasis cannot supply parameters yet; naming one with `--game` or `--opponent` reports which parameters it needs.

## A game

Subclass `oryx.Game` with an `id` and the class registers itself when the file is loaded. Typed class fields are the game's parameters; the class value is the default, and the first line of the docstring is the description.

```python
import oryx


class NimState(oryx.State):
    def __init__(self, stones, max_take):
        self.stones = stones
        self.max_take = max_take
        self.player = 0

    def legal_actions(self):
        return list(range(1, min(self.max_take, self.stones) + 1))

    def apply(self, action):
        self.stones -= action
        self.player = 1 - self.player

    def undo(self, action):
        self.stones += action
        self.player = 1 - self.player

    def current_player(self):
        return self.player

    def is_terminal(self):
        return self.stones == 0

    def outcome(self):
        rewards = [0.0, 0.0]
        if self.stones == 0:
            rewards = [-1.0, -1.0]
            rewards[1 - self.player] = 1.0
        return rewards


class Nim(oryx.Game, id="nim"):
    """Players alternate taking 1..max_take stones from a pile; whoever takes the last stone wins."""

    stones: int = 21
    max_take: int = 3
    num_players = 2

    def new_initial_state(self):
        return NimState(self.stones, self.max_take)
```

An action is an integer (`ActionId`); here it is the number of stones taken. `outcome()` returns one reward per player. `action_to_string` is optional and is what the console lists for a move.

## A strategy

A strategy implements `decide(context)` and returns one of the state's legal actions. The state it receives is lent for the call: it can be applied and undone, but it must not be kept after `decide()` returns.

```python
class MonteCarlo(oryx.Strategy, id="monte-carlo"):
    """Flat Monte Carlo: scores each legal action by the mean reward of random playouts."""

    playouts: int = 30
    seed: int = 1

    def __init__(self):
        self.random = oryx.Random(self.seed)

    def decide(self, context):
        state = context.state
        player = state.current_player()
        return max(state.legal_actions(), key=lambda action: self.score(state, action, player))
```

Because a strategy only uses the state interface, one script plays every game, whether the game is C++ or Python. A strategy that only understands one game's state should be named `<game>/<name>`, and Oasis then offers it for that game only.

## Where scripts are found

A script is a plain `.py` file under a **script root**. The roots come from the settings file, `oryx.yaml`. Oasis reads `Oasis/oryx.yaml` (relative to the working directory, which is the repository root), or the file named by `--settings=<file>`; an application that registers no file of its own gets `oryx.yaml` from the working directory. `Oasis/oryx.yaml` is:

```yaml
scripting:
  roots:
    - scripts
```

Relative entries are relative to the settings file. You can add roots for one run with `--script-root <dir>`, load a single file with `--script <file>` (a file outside every root is imported from its own directory), or import an installed module with `--module <name>`. Nothing is scanned when no root is configured, and a root that does not exist is warned about and skipped.

Every `.py` file under a root is loaded, in path order, and imported by its real dotted name (`Oasis/scripts/nim.py` is `nim`, `scripts/deeper/alpha.py` is `deeper.alpha`). Each root is on `sys.path`, so scripts can import each other, and the virtual environment's `site-packages` is on it too, so a script can import a package you installed with `uv`. A file or directory whose name starts with `_` or `.` is a helper: other scripts can import it (`import _common`) but it is never loaded on its own.

A script that a standard-library module or another root shadows (a `random.py`, or two roots that both define `nim.py`) is reported with the file and the module that won; rename the script. A script's name cannot contain a dot.

A script also runs on its own with `python nim.py` when `oryx` is importable. The settings file is checked as it loads: an unknown section or key is warned about with its line, and a malformed file or a wrong value is reported and Oasis runs on the defaults.

## When a script goes wrong

An exception in a script never takes Oasis down as an unresponsive process. A script that fails to import is logged with its Python traceback and skipped; the remaining scripts still load. A method that raises during a match ends that match: the traceback is logged and Oasis exits with a non-zero status (a strategy that returns an illegal action, including the invalid-action sentinel, is reported the same way, and a method that returns the wrong type says which method and which type). Registering an id that already exists is an error unless the class asks for it with `id="nim", overwrite=True`; loading the same script again replaces its own entries.

## Reloading

Reloading reloads the settings file, re-runs discovery and loads every script again, so edited scripts take effect, new files and new roots appear, and entries a script no longer defines disappear. Helper modules are imported again too, so what they register comes back. It is triggered by posting a `ReloadScriptsEvent` to the application, which `ScriptingLayer` handles; Oasis has no command for it yet. A script with an error is logged and its entries stay gone until it is fixed, while the others reload. An `oryx.yaml` that no longer parses keeps the previous settings.

## Helpers for scripts

* `oryx.debug`: `trace`, `info`, `warn`, `error` and `critical` write to Oasis's client log (the text is never a format string), `install()` sends the standard `logging` module there too, and `check(condition, message)` raises `OryxAssertionError` in every build instead of stopping the process.
* `oryx.math`: `Vec2/3/4`, `Mat2/3/4` and scalar helpers (`clamp`, `lerp`, `radians`, ...), all over floats.
* `oryx.simulate(...)` returns a `BatchResult` with win rates, mean rewards, the run's metadata and `to_dict()`; `oryx.benchmark.benchmark(...)` adds timing and throughput. `to_numpy()` and `to_dataframe()` need numpy or pandas installed (`uv sync` installs them for development).

## Speed

A game or strategy written in Python is correct but slow inside a hot loop: every `apply`, `undo` and `legal_actions` call crosses from C++ into Python (tens of nanoseconds each, and `legal_actions` is remembered until the next `apply` or `undo`, so a state should change only through them). C++ games with Python strategies, and batches where every participant is C++, stay on the fast path. Prototype in Python, then port to C++ behind the same id: the parameters and the registry id stay the same, so nothing that names the game changes, and running both under identical seeds and comparing the batch results shows whether the port agrees.

## Not yet

* Installed packages that provide games through Python entry points, until packages can be installed.
* Game parameters on the Oasis command line: a game is created with its defaults.
* `import oryx` from a REPL or notebook: that research host comes last in Phase 7.

See the [Python API design](../design/python-api.md) for the decisions behind all of this.
