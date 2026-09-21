# Scripting in Oasis

A game or strategy written in Python runs in Oasis exactly like a C++ one: it registers under an id, shows up in the menu with its description and parameters, and plays against any C++ or Python counterpart. Oasis embeds a Python interpreter, loads your scripts at start-up and never needs a rebuild when a script changes.

Python is on by default. Build with `--no-python` to leave it out; Oasis then has only the C++ games and strategies.

## Try it

The repository ships two example scripts in `Oasis/scripts/`: Nim and a Monte Carlo strategy that plays both Nim and TicTacToe.

```bash
uv run build build run                                   # menu: pick a game, then an opponent
uv run build build run --game nim --opponent monte-carlo # play Nim against the Python strategy
uv run build build run --game nim --simulate=monte-carlo,first-legal,20
uv run build build run --simulate=monte-carlo,tictactoe/heuristic,20
```

The menu prints each entry's description and its parameters with their defaults, for example `nim - Players alternate ... [stones=21, max_take=3]`.

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

Sources are combined and de-duplicated in this order:

1. `--script <file>` and `--module <name>` on the command line.
2. The `ORYX_SCRIPT_PATH` list of files and directories. `uv run build` sets it from `[scripting] paths` in `oryx.toml` and `oryx.local.toml`, and puts it in the generated VS Code launch configuration.
3. A recursive scan of the working directory for `*.oryx.py`, skipping `.git`, virtual environments, `build/` and `bin*/`. Put a script anywhere in the repository and it is found with no configuration.

The project root and the virtual environment's `site-packages` are on the interpreter's `sys.path`, so a script can import a helper module that sits in the project or a package you installed with `uv`.

## When a script goes wrong

An exception in a script never takes Oasis down. A script that fails to import is logged with its Python traceback and skipped; the remaining scripts still load. A method that raises during a match ends that match with the traceback, and a strategy that returns an illegal action is reported the same way. Registering an id that already exists is an error unless the class asks for it with `id="nim", overwrite=True`; loading the same script again simply replaces its own entries.

## Reloading

Reloading re-runs discovery and loads every script again, so edited scripts take effect and new files appear; entries a script no longer defines disappear. It is triggered by posting a `ReloadScriptsEvent` to the application, which `ScriptingLayer` handles. Oasis has no command for it yet. Helper modules that a script imports are not re-imported.

## Speed

A game or strategy written in Python is correct but slow inside a hot loop: every `apply`, `undo` and `legal_actions` call crosses from C++ into Python. C++ games with Python strategies, and batches where every participant is C++, stay on the fast path. Prototype in Python, then port to C++ behind the same id: the parameters and the registry id stay the same, so nothing that names the game changes, and running both under identical seeds and comparing the batch results shows whether the port agrees.

## Not yet

* Installed packages that provide games through Python entry points, until packages can be installed.
* Game parameters on the Oasis command line: a game is created with its defaults.
* `import oryx` from a REPL or notebook: that research host comes last in Phase 7.

See the [Python API design](../design/python-api.md) for the decisions behind all of this.
