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
| Hosts | Both. Python-as-host (`import oryx`, notebooks) and C++-as-host (Oasis embeds an interpreter through a Python runtime). One binding source, `bind_oryx`, is compiled both as the `_oryx` extension module and into `Oryx` for embedding. One process has one registry, so a script-registered game reaches Oasis's menu | Working decision |
| Build option | Python is on by default and opt-out at build time (`--no-python` / `python = false` in `oryx.toml`). Only `Oryx/backends/Python/` and the `OryxPy` project need pybind11 and `<Python.h>`; when off, Premake excludes both and the rest of Oryx is untouched. CI gets a Python-off leg | Working decision |
| pybind11 | Built by Premake; vendored as a git submodule at `Oryx/vendor/pybind11` like every other vendored library ([Platform](platform.md#source-layout-public-api-and-backends)) | Working decision |
| Extension module | `OryxPy` is a small Premake `SharedLib` producing `_oryx`, which `import oryx` loads outside Oasis (Python can only import shared libraries; Oryx is a static library). It lives beside `Oasis`, whose game sources it compiles directly, and is skipped when Python is off. A user's own C++ games would use a Premake helper that builds a superset module (Oryx plus their games); a shared `libOryx` is the long-term alternative. Exact layout is decided when the build integration lands | Working decision (layout open until then) |
| Private backend | The Python implementation lives in `Oryx/backends/Python/` (`PythonRuntime`, the `PyScripted*` adapters, `bind_oryx`), private per [Source Layout](platform.md#source-layout-public-api-and-backends) | Working decision |
| Games in Oasis | TicTacToe and its heuristic stay in `Oasis`; `OryxPy` compiles those sources directly, the same precedent as `Tests` | Working decision |
| numpy | Optional (`oryx[numpy]`): bulk numeric data (rewards, batch results, `Vec`/`Mat`) becomes an `ndarray` when installed; tiny data such as `legal_actions()` stays a plain list | Working decision |
| Config | `[python] enabled` (build option, default true) and `[scripting] paths` (run-time list; `oryx.local.toml` adds personal paths) in `oryx.toml`. Only `build_system` reads TOML; C++ never parses it | Working decision |

### API shape and registration

| Topic | Position | Status |
| ----- | -------- | ------ |
| Class roles | Public Python ABCs `Game`/`State`/`Strategy` mirror `IGame`/`IState`/`IStrategy`. Private wrappers `PyGame`/`PyState`/`PyStrategy` hold a C++ object. C++ classes backed by a script, `PyScriptedGame`/`PyScriptedState`/`PyScriptedStrategy`, implement the language-agnostic `IScriptedGame`/`IScriptedState`/`IScriptedStrategy` from `Scripting/`. Members beyond `origin()` (and `param_schema()` on the game) are decided when the Python backend is written | Working decision |
| Registries | Gymnasium-style: `oryx.make_game("tictactoe")`, `oryx.make_strategy("random", seed=1)`. Python registrations go into the same `Registry<T>` as C++ ones, so Oasis's menu lists them | Working decision |
| Construction parameters | `Registry<T>::create(name, Params)`; `Params` is a string-keyed map of `bool`/`int64`/`double`/`string`; ids are unversioned. Every registration, C++ or Python, declares a schema (keys, types, defaults) so validation, `describe()` and Oasis's menu treat both alike. The registry stores the schema and a description per entry (`EntryInfo`); a bad key or type is an error naming the key | Working decision |
| Unity-style registration | `class Nim(oryx.Game, id="nim")` registers on import via `__init_subclass__`; typed class fields (`stones: int = 21`) form the schema. `oryx.register_game(...)` remains for factories and lambdas | Working decision |
| Origin-tagged registration | Research needs notebook-safe re-registration: re-running a cell that redefines `class Nim(oryx.Game, id="nim")` replaces the entry when the origin (language, module, source file) is the same; a clash with an entry of another origin, including a C++ one, is an error unless `overwrite=True`. This lives in a scripting-specific registry over `Registry<IGame>`/`Registry<IStrategy>`, not in the core `Registry<T>`, and is built when the first script-defined registrant exists | Working decision (built with the script-backed types) |
| Seeding | `simulate(seed=S)`: strategies created by name that declare a `seed` parameter get `S + seat_index`; instances passed in are user-seeded; no change to `IStrategy` or `BatchRunner`; the master seed is recorded in the results | Working decision |
| Logging and assertions | Exposed as Tier 1 modules. `oryx.log` routes to the client logger so script output interleaves with engine output; `oryx.assertions.check(cond, msg)` logs then raises `OryxAssertionError`, because `OX_ASSERT` traps the process in Debug and would kill the interpreter with no traceback | Working decision |
| Initialisation and guard | Notebooks and the REPL call `oryx.init()` (it binds the idempotent `oryx::init()`); scripts embedded in Oasis never do, because Oasis's `main()` already did. Only the entry points every session must pass through, `make_game` and `make_strategy`, are guarded: a guard raises `OryxError` naming the function (`make_game() was called before oryx.init()`) instead of lazily initialising. The guard is a zero-lambda NTTP wrapper exposed through a `GUARDED_FUNC(fn)` macro; its check is one inline load with an `[[unlikely]]` branch and an out-of-line cold throw. The message goes in the exception, not the log, because the guard fires exactly when no logger exists. Lands with the bindings (step 5) | Working decision |
| Module tiers | Tier 1: game, strategy, registry, simulation (`Match`/`BatchRunner`/`BatchResult` and `simulate()`), random, results, log, assertions. Tier 2: math (`Vec`/`Mat` and ndarray), benchmark. Not exposed: `Application`, `Layer`, `Event` | Working direction |

### The scripting seam

| Topic | Position | Status |
| ----- | -------- | ------ |
| `Scripting/` module | `Oryx/src/Oryx/Scripting/` is a full, language-agnostic module, always compiled and free of any Python dependency: `IScriptRuntime` (start/stop, load, reload, file patterns, language), `ScriptSource` and `ScriptOrigin` (data-only), a free `discover_scripts()`, `ScriptError`, `IScriptedGame`/`IScriptedState`/`IScriptedStrategy`, `ScriptRuntimeRegistry` and a public `ScriptingLayer : Layer`. The Python backend builds on it | Working decision |
| Built before a second implementation | The repository's precedent (`IBoard`, the `Registry<T>` timing) is to wait for a second implementation before extracting an interface. This seam is built with one implementation (Python) **by choice**: it keeps public headers and applications free of language names and confines the Python on/off switch to the backend, so a further runtime needs no public change. It is a deliberate exception to the [Design Review Principle](principles.md#design-review-principle), not a new default | Working decision (deliberate exception) |
| Runtime lookup | `PythonRuntime` registers itself as `"python"` in `ScriptRuntimeRegistry` (`OX_REGISTER_SCRIPT_RUNTIME`), alongside `OX_REGISTER_GAME`/`OX_REGISTER_STRATEGY`. `ScriptingLayer` is deliberately simple: it discovers sources, creates one runtime per discovered language from the registry and loads the sources. There is no `PythonScriptingLayer` and no Python macro in public headers; Oasis names no Python type. With Python off no `"python"` runtime registers | Working decision |
| Script discovery | Sources are unioned and deduplicated from: (1) `--script <file>` / `--module <name>`; (2) `ORYX_SCRIPT_PATH` (an OS path-separator list, set by `build_system` from `[scripting] paths`); (3) a zero-config recursive scan of the working directory for each runtime's file pattern (`*.oryx.py` for Python), skipping `.git`, virtual environments, `build/` and `bin*/`; (4) installed packages through Python entry points, handled by `PythonRuntime` itself. Under plain `import oryx` no discovery is needed: the user's own `import my_game` registers it. Because patterns come from the registered runtimes, the scan finds nothing when none is registered, and a "no runtime for `.py`" message is logged only for explicitly named sources | Working decision |

### Ownership, lifetime and errors

| Topic | Position | Status |
| ----- | -------- | ------ |
| Ownership and lifetime | Scoped to scripting. Adapters (`PyScripted*`) hold a `py::object` and forward under the GIL. A borrowed `IState&` or `Context` never outlives `decide()`: the wrapper invalidates the Python-visible state afterwards. Factories held by Python release their `py::object` before interpreter finalisation. Still open: how a pure-Python host owns C++ objects handed back to it, and finalisation ordering details | Working decision (scoped to scripting) |
| Script errors | A script exception becomes a `ScriptError` carrying the traceback, propagates to a layer boundary and is logged there ([Error Handling](cpp-api.md#error-handling)); it aborts that match or load and never terminates Oasis | Working decision |
| Speed | A Python-defined `State` inside C++ Minimax crosses the boundary on every `apply`/`undo`/`legal_actions`: correct but slow. C++ games with Python strategies, and all-C++ batches, are the fast paths | Documented limit |

### Research and prototyping

Research use is a first-class requirement, not a by-product:

* **Frictionless import:** the normal build places `_oryx` into the `python/oryx/` package so `uv run python`, Jupyter and the REPL can `import oryx` with no path work. Wheels and PyPI are a later packaging phase.
* **Interactive stepping:** the Python `Match` exposes `decide()`/`apply()`/`undo()`/`redo()`/`history`, as the C++ `Match` does.
* **One-liner simulation:** `oryx.simulate(game, strategies, games=10_000, seed=...)` returns a `BatchResult`; this covers Phase 7's "configure experiments / access statistics" minimally, and full `Experiment` and sweeps stay Phase 8.
* **Results as data:** `BatchResult` gets `__repr__`/`_repr_html_`, `to_dict()`, numpy arrays, win rates and mean rewards, an optional `to_dataframe()`, and records the seed, configuration and version.
* **Long runs stay usable:** all-C++ batches release the GIL, and chunked execution lets Ctrl-C or a notebook interrupt work.
* **Introspection:** `list_games()`, `list_strategies()`, `describe("nim")` (schema and origin), docstrings and `.pyi` stubs.
* **Graduation path:** a Python prototype and its C++ port share the ABC contract, the registry id and the schema, so call sites do not change; differential testing (same seeds, both implementations, compare `BatchResult`) validates a port.

### Open questions

```text
Decision: Open (assertion hook)
Options: an assertion-handler hook in Core/Assert.h / leave asserts trapping
Reason unresolved: OX_ASSERT traps in Debug, so a C++ assert reached from a script would kill the interpreter
Next step: decide with the logging/assertions bindings
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
Decision: Open (capabilities from Python)
Options: named capabilities on the Context wrapper / type-indexed access as in C++
Reason unresolved: IStrategy::required_capabilities() returns std::type_index, which Python cannot name, and only IActionFeatures exists today
Next step: decide with the script-backed types
```

```text
Decision: Open (OryxPy and Oasis's precompiled header)
Options: compile Oasis game sources with ospch.h / a separate OryxPy pch / no pch for those sources
Reason unresolved: OryxPy compiles Oasis's game sources, which expect Oasis's precompiled header
Next step: decide with the build integration
```
