# Python API

## Python API Design

Python should not simply mirror C++.

A Python user should be able to work at a higher conceptual level.

For example:

```python
experiment = Experiment(
    game=game,
    strategies=[random_strategy, mcts_strategy],
    games=10_000,
)

results = experiment.run()
```

This is illustrative rather than a proposed final API.

The actual API should be designed after the C++ core abstractions are clearer.

## Bindings

pybind11 is the current intended binding technology.

The binding layer should act as an API boundary rather than exposing every internal C++ type.

Internal C++ implementation details should remain internal where possible.

This reduces Python API churn and allows the C++ implementation to evolve independently.

## Phase 7: Scripting and Research

Phase 7 ([Roadmap](../roadmap.md#9-phase-7-python-research-layer)) is designed around two uses of one `oryx` API:

* **Scripting, Unity-style:** a game or strategy defined in a Python file runs inside the C++ `Oasis` executable exactly like a C++ one.
* **Research and prototyping:** `import oryx` from a REPL, script or notebook, with a Python prototype later portable to C++ behind the same registry id.

The milestone that closes the phase: **Nim written in Python, inside Oasis, plus one Python strategy that plays both Nim and TicTacToe.** Nim is a pile of `stones` (default 21) from which players alternate taking 1..`max_take` (default 3); the last taker wins. It replaced Rock-Paper-Scissors, which is simultaneous-move and hidden-information and so unsupported by the strictly-alternating, fully-visible engine ([Architecture §14](../architecture.md#14-current-architectural-unknowns)); RPS stays a later test of that open topic.

Status legend as in the [Decision Log](decision-log.md). The decisions below are working decisions from the Phase 7 brainstorm; the implementation steps that validate them are noted where relevant.

### Hosts and build

| Topic | Position | Status |
| ----- | -------- | ------ |
| Hosts | Both, in stages. **Embedded first** (C++-as-host): Oasis embeds an interpreter through a Python runtime, and Python runs as scripting inside Oasis. The **research host** comes last (Python-as-host): `import oryx` from a REPL, script or notebook. One binding source, `bind_oryx`, is compiled into `Oryx` for embedding and later also becomes the `_oryx` extension module. One process has one registry, so a script-registered game reaches Oasis's menu | Working decision (staged) |
| Build option | Python is on by default and opt-out at build time (`--no-python` / `python = false` in `oryx.toml`). Only `Oryx/backends/Python/` needs pybind11 and `<Python.h>`; when off, Premake adds no Python file, include path, define or link and the rest of Oryx is untouched. `build_system` reads the interpreter's include directory, libpython and prefix from `sysconfig` and passes them to Premake as `--python-*` options (`premake/python.lua`); changing them clears the previous binaries, because Makefiles do not rebuild an object whose defines changed. `Oasis` and `Tests` link libpython through `linkPython()`, since their whole-archive link of `Oryx` pulls in the backend. CI has a Python-off leg | Working decision |
| pybind11 | Built by Premake; vendored as a git submodule at `Oryx/vendor/pybind11` like every other vendored library ([Platform](platform.md#source-layout-public-api-and-backends)) | Working decision |
| Extension module | Research host, last stage. Python can only import shared libraries and Oryx is a static library, so `import oryx` outside Oasis needs a `_oryx` shared library. It is **owned by Oasis** (Oasis is the experiment ground and the example for others) and there is no separate `OryxPy` project; its Premake shape, and how it reuses Oasis's precompiled header, are decided when that stage starts. A user's own C++ games would use a Premake helper that builds a superset module (Oryx plus their games); a shared `libOryx` is the long-term alternative | Working decision (layout open until the research stage) |
| Private backend | The Python implementation lives in `Oryx/backends/Python/` (`PythonRuntime`, the `PyScripted*` adapters, `bind_oryx`), private per [Source Layout](platform.md#source-layout-public-api-and-backends) | Working decision |
| Embedded interpreter | `PythonRuntime` starts CPython from a `PyConfig` with no signal handlers and no bytecode written into the user's tree. The interpreter prefix (`OX_PYTHON_HOME`, unless `PYTHONHOME` is set) and the Python package directory (`Oryx/backends/Python`, holding the pure-Python `oryx/` package) (`OX_PYTHON_PACKAGE_DIR`, prepended to `sys.path`) are baked into the backend at configure time, so Tests, Oasis and the IDE work with no environment setup; the binaries are machine-local. The native module `_oryx` is registered explicitly before the interpreter starts (`PyImport_AppendInittab`), not by a static `PYBIND11_EMBEDDED_MODULE`, and the pure-Python `oryx` package re-exports it. The venv's site-packages (`OX_PYTHON_SITE_PACKAGES`, read from `sysconfig` at configure time) and the project root (the working directory at start, the same root the discovery scan uses) are *appended* to `sys.path`, so scripts can `import numpy` and helper modules that sit next to the project, and neither can shadow the standard library or `oryx` | Working decision (built) |
| Games in Oasis | TicTacToe and its heuristic stay in `Oasis`; the research module, being Oasis-owned, compiles those sources directly, the same precedent as `Tests` | Working decision |
| numpy | Optional (`oryx[numpy]`): bulk numeric data (rewards, batch results, `Vec`/`Mat`) becomes an `ndarray` when installed; tiny data such as `legal_actions()` stays a plain list | Working decision |
| Config | `[python] enabled` (build option, default true) and `[scripting] paths` (run-time list; `oryx.local.toml` adds personal paths) in `oryx.toml`. Only `build_system` reads TOML; C++ never parses it | Working decision |

### API shape and registration

| Topic | Position | Status |
| ----- | -------- | ------ |
| Class roles | `oryx.Game`, `oryx.State` and `oryx.Strategy` are plain Python classes created by the native module (no `.py` file, no pybind11 trampolines): `Game` and `Strategy` carry a native `__init_subclass__` that registers subclasses, `State` supplies a default `action_to_string()` and is optional (states are duck-typed). Objects owned by the engine come back as `oryx.GameHandle`/`StateHandle`/`StrategyHandle`, the Python faces of the private wrappers `PyGame`/`PyState`/`PyStrategy` (the engine interfaces themselves are never bound). C++ classes backed by a script, `PyScriptedGame`/`PyScriptedState`/`PyScriptedStrategy`, implement the language-agnostic `IScriptedGame`/`IScriptedState`/`IScriptedStrategy` from `Scripting/` and hold the user's object, calling its methods by name under the GIL. A `Game` defines `new_initial_state()`; `num_players` and an optional `name` are unannotated class attributes read once when the game is created. A `State` defines `legal_actions()` (a list of ints), `apply`, `undo`, `current_player`, `is_terminal`, `action_to_string` and `outcome()`, which returns a **list of rewards**, one per player (the terminal flag comes from `is_terminal()`, so there is one source of truth). A `Strategy` defines `decide(context)` returning an action | Working decision |
| Registries | Gymnasium-style: `oryx.make_game("tictactoe")`, `oryx.make_strategy("random", seed=1)`. Python registrations go into the same `Registry<T>` as C++ ones, so Oasis's menu lists them | Working decision |
| Core bindings | `_oryx` exposes `make_game`/`make_strategy` (keyword arguments are the parameters), `list_games`/`list_strategies`, `describe_game`/`describe_strategy` (description, parameter schema, origin or `None` for C++), `Match` (`state`, `is_terminal`, `current_player`, `outcome`, `decide`, `apply`, `undo`, `redo`, `play`, `history`), `BatchRunner`, a read-only `BatchResult` (`matches`, `wins`, `draws`, `rewards`, `decisions`), `simulate(game, strategies, games, seed)` and `Random`. A game or strategy argument is a registry name, a native object or an instance of a Python subclass. All entry points that would touch an uninitialised logger are guarded ([Initialisation and guard](#api-shape-and-registration)). `BatchResult` recording its seed, `to_dict` and `_repr_html_` belong to the results-as-data work | Working decision (built) |
| Construction parameters | `Registry<T>::create(name, Params)`; `Params` is a string-keyed map of `bool`/`int64`/`double`/`string`; ids are unversioned. Every registration, C++ or Python, declares a schema (keys, types, defaults) so validation, `describe()` and Oasis's menu treat both alike. The registry stores the schema and a description per entry (`EntryInfo`); a bad key or type is an error naming the key | Working decision |
| Unity-style registration | `class Nim(oryx.Game, id="nim")` registers on import through `__init_subclass__` (`overwrite=True` allowed; a subclass without an `id` is an intermediate base and is not registered). Fields annotated `bool`/`int`/`float`/`str` form the schema, a class value is the default and no value means required; anything else needs `typing.ClassVar`, and `name`/`num_players` are reserved. The first line of the docstring is the description. A missing `new_initial_state()`/`decide()` is a `ScriptError` at import. Creating an entry runs `cls.__new__`, sets the resolved parameters as attributes, then calls `__init__()` (no arguments), so `__init__` can read its parameters. `oryx.register_game(id, factory, params=..., description=..., overwrite=...)` and `register_strategy` register factory functions, which receive the parameters as keywords; `params` maps a name to its default, or to `bool`/`int`/`float`/`str` for a required one | Working decision (built) |
| Origin-tagged registration | Research needs notebook-safe re-registration: re-running a cell that redefines `class Nim(oryx.Game, id="nim")` replaces the entry when the origin (language, module, source file) is the same; a clash with an entry of another origin, including a C++ one, is a `ScriptError` unless `overwrite=True`. Built in `Scripting/ScriptRegistry.h` as free functions over `Registry<IGame>`/`Registry<IStrategy>` with a side table of origins, not in the core `Registry<T>`. `unregister_scripted(language)` drops a language's entries, and `PythonRuntime::stop()` calls it before finalising the interpreter so no factory outlives the objects it holds. Overwriting a C++ entry removes it for good once the runtime stops | Working decision (built) |
| Seeding | `simulate(seed=S)`: strategies created by name that declare a `seed` parameter get `S + seat_index`; instances passed in are user-seeded; no change to `IStrategy` or `BatchRunner`; the master seed is recorded in the results | Working decision |
| Logging and assertions | Exposed as Tier 1 modules. The language-agnostic parts live in `Scripting/`: `script_log(level, message)` (always formatted with `"{}"`, so script text is never a format string) and `script_check(condition, message)`, which logs then throws `AssertionError` (`Core/Error.h`) and works in every profile. The Python backend only binds them: `oryx.log.*` routes to the client logger so script output interleaves with engine output (plus a `logging.Handler` bridge, `oryx.log.install()`), and `oryx.assertions.check` raises `OryxAssertionError`. A C++ `OX_ASSERT` is a separate matter, see the assertion hook below | Working decision |
| Initialisation and guard | The guard machinery is language-agnostic and lives in `Scripting/InitGuard.h`: `OX_GUARDED_FUNC(function, name)` yields a function pointer with the same signature whose check is one inline load with an `[[unlikely]]` branch and an out-of-line cold throw of `oryx::Error` (`oryx.log.info() was called before Oryx was initialised`). It covers every entry point that would otherwise dereference a logger that does not exist yet: `oryx.log.*` and `oryx.assertions.check` now (an unguarded pre-init `oryx.log.info()` would be a null dereference), `make_game`/`make_strategy` when they land. The message goes in the exception, not the log, because the guard fires exactly when no logger exists. Free functions only; the fixed `Args...` signature is required so pybind11 can deduce the Python signature. Scripts embedded in Oasis never call `oryx.init()`, because Oasis's `main()` already did; the `oryx.init()` binding (the idempotent `oryx::init()`) arrives with the research host | Working decision |
| Module tiers | Tier 1: game, strategy, registry, simulation (`Match`/`BatchRunner`/`BatchResult` and `simulate()`), random, results, log, assertions. Tier 2: math (`Vec`/`Mat` and ndarray), benchmark. Not exposed: `Application`, `Layer`, `Event` | Working direction |

### The scripting seam

| Topic | Position | Status |
| ----- | -------- | ------ |
| `Scripting/` module | `Oryx/src/Oryx/Scripting/` is a full, language-agnostic module, always compiled and free of any Python dependency: `IScriptRuntime` (start/stop, load, reload, unload, file patterns, language), `ScriptSource` and `ScriptOrigin` (data-only), a free `discover_scripts()`, `ScriptError`, `IScriptedGame`/`IScriptedState`/`IScriptedStrategy`, `ScriptRuntimeRegistry` and a public `ScriptingLayer : Layer`. The Python backend builds on it | Working decision |
| Built before a second implementation | The repository's precedent (`IBoard`, the `Registry<T>` timing) is to wait for a second implementation before extracting an interface. This seam is built with one implementation (Python) **by choice**: it keeps public headers and applications free of language names and confines the Python on/off switch to the backend, so a further runtime needs no public change. It is a deliberate exception to the [Design Review Principle](principles.md#design-review-principle), not a new default | Working decision (deliberate exception) |
| Reload | `ReloadScriptsEvent` (`Events/ScriptEvent.h`) is handled by `ScriptingLayer`, which re-runs discovery, calls `IScriptRuntime::unload()` on each running runtime (`PythonRuntime`: drop every scripted registration) and then `reload()` for each source, starting a runtime that had no sources before. A source that fails is logged and the rest still reload. Games and strategies already in play keep their Python objects, only new creations see the new definitions. Limits: helper modules a script imports are not re-imported, and Oasis has no trigger yet (the event is posted by whatever hosts the layer; a console or GUI command belongs with the input work) | Working decision (built) |
| Runtime lookup | `PythonRuntime` registers itself as `"python"` in `ScriptRuntimeRegistry` (`OX_REGISTER_SCRIPT_RUNTIME`), alongside `OX_REGISTER_GAME`/`OX_REGISTER_STRATEGY`. `ScriptingLayer` is deliberately simple: it discovers sources, creates one runtime per discovered language from the registry and loads the sources; a runtime is started only when it has sources. There is no `PythonScriptingLayer` and no Python macro in public headers; Oasis names no Python type. With Python off no `"python"` runtime registers | Working decision |
| Script discovery | Sources are unioned and deduplicated from: (1) `--script <file>` / `--module <name>`; (2) `ORYX_SCRIPT_PATH` (an OS path-separator list, set by `build_system` from `[scripting] paths`); (3) a zero-config recursive scan of the working directory for each runtime's file pattern (`*.oryx.py` for Python), skipping `.git`, virtual environments, `build/` and `bin*/`; (4) installed packages through Python entry points (`oryx.games`/`oryx.strategies`), **deferred until packages can be installed** (wheels/PyPI, out of Phase 7): finding them needs a running interpreter, so it would make `ScriptingLayer` start every runtime on every launch even with no scripts. Under plain `import oryx` no discovery is needed: the user's own `import my_game` registers it. Because patterns come from the registered runtimes, the scan finds nothing when none is registered, and a "no runtime for `.py`" message is logged only for explicitly named sources | Working decision |

### Ownership, lifetime and errors

| Topic | Position | Status |
| ----- | -------- | ------ |
| Ownership and lifetime | Scoped to scripting. Adapters (`PyScripted*`) hold a `ScriptObject`, which releases its `py::object` under the GIL and leaks it instead once the interpreter is finalised, and forward calls under `py::gil_scoped_acquire`. A `State` or `Context` lent to a strategy is a handle that is invalidated when `decide()` returns; later use raises `OryxError`. Factories held by Python are dropped by `unregister_scripted` before finalisation. Still open: how a pure-Python host owns C++ objects handed back to it, and finalisation ordering details, notably an adapter that outlives `PythonRuntime::stop()` (its methods must not be called afterwards) | Working decision (scoped to scripting) |
| Script errors | A script exception raised inside a Python-backed game, state or strategy becomes a `ScriptError` carrying the traceback, propagates to a layer boundary and is logged there ([Error Handling](cpp-api.md#error-handling)); it aborts that match or load and never terminates Oasis. A Python caller of `simulate()` or `Match` sees `oryx.ScriptError` (with `.detail`), not the original exception type. The binding never lets a script reach an assertion or undefined behaviour: `apply()` on a native or lent state and `Match.apply()` check the action against `legal_actions()`, and a Python strategy that returns an illegal action is a `ScriptError` | Working decision |
| Speed | A Python-defined `State` inside C++ Minimax crosses the boundary on every `apply`/`undo`/`legal_actions`: correct but slow. C++ games with Python strategies, and all-C++ batches, are the fast paths. `simulate`, `BatchRunner.run` and `Match.play` release the GIL when no participant is script-backed (`dynamic_cast` to `IScriptedGame`/`IScriptedStrategy`); the engine is single-threaded, so adapters rely on the reentrant GIL. `apply()` on a native state pays for a `legal_actions()` check | Documented limit |

### Research and prototyping

Research use is a first-class requirement, not a by-product:

* **Frictionless import (last stage):** the normal build places `_oryx` into the `Oryx/backends/Python/oryx/` package so `uv run python`, Jupyter and the REPL can `import oryx` with no path work. Wheels and PyPI are a later packaging phase.
* **Interactive stepping:** the Python `Match` exposes `decide()`/`apply()`/`undo()`/`redo()`/`history`, as the C++ `Match` does.
* **One-liner simulation:** `oryx.simulate(game, strategies, games=10_000, seed=...)` returns a `BatchResult`; this covers Phase 7's "configure experiments / access statistics" minimally, and full `Experiment` and sweeps stay Phase 8.
* **Results as data:** `BatchResult` gets `__repr__`/`_repr_html_`, `to_dict()`, numpy arrays, win rates and mean rewards, an optional `to_dataframe()`, and records the seed, configuration and version.
* **Long runs stay usable:** all-C++ batches release the GIL, and chunked execution lets Ctrl-C or a notebook interrupt work.
* **Introspection:** `list_games()`, `list_strategies()`, `describe("nim")` (schema and origin), docstrings and `.pyi` stubs.
* **Graduation path:** a Python prototype and its C++ port share the ABC contract, the registry id and the schema, so call sites do not change; differential testing (same seeds, both implementations, compare `BatchResult`) validates a port.

### Open questions

```text
Decision: Decided, built with the research host (assertion hook)
Choice: a replaceable handler in Core/Assert.h (default unchanged: log and trap). Importing the _oryx extension installs one that throws, so a C++ assert reached from Python surfaces as OryxAssertionError; embedded Oasis never installs it and keeps trapping in the debugger
Why later: nothing in the embedded host installs it, so it has no user until the research host
Next step: build it with the research host
```

```text
Decision: Open (long-batch interruption)
Options: fixed chunk size / time-sliced chunks
Reason unresolved: BatchRunner::run(n) is monolithic today, and Ctrl-C needs PyErr_CheckSignals between chunks
Next step: decide with the Tier 2 and polish work
```

```text
Decision: Open (scan exclusions)
Options: fixed skip list / configurable list / a --no-scan flag
Reason unresolved: the zero-config scan skips only .git, virtual environments, build/ and bin*/
Next step: decide with the polish work
```

```text
Decision: Deferred (capabilities from Python)
Options: named capabilities on the Context wrapper / type-indexed access as in C++
Reason unresolved: IStrategy::required_capabilities() returns std::type_index, which Python cannot name, and only IActionFeatures exists today
Current behaviour: a Python strategy cannot declare capabilities, so none are validated; context.action_features exposes the game's IActionFeatures (None when it has none)
Next step: decide with a second capability, most likely as a small name-to-type table in Scripting/
```

```text
Decision: Open (extension module and Oasis's precompiled header)
Options: compile Oasis game sources into the module with ospch.h / a separate pch / no pch for those sources
Reason unresolved: the Oasis-owned research module compiles Oasis's game sources, which expect Oasis's precompiled header
Next step: decide when the research host starts
```
