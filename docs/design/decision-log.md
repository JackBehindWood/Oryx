# Decision Log

## Current Decision Log

| Decision                        | Current position      | Status                |
| ------------------------------- | --------------------- | --------------------- |
| Primary engine language         | C++                   | Working decision      |
| Research/tooling language       | Python                | Working decision      |
| Python bindings                 | pybind11              | Working decision      |
| C++ build system                | Premake5              | Working decision      |
| C++ test framework              | doctest (git submodule, `tests/vendor/doctest`) | Working decision |
| Developer CLI                   | Python `build`        | Working decision      |
| Documentation site              | MkDocs Material built from `docs/` via `build docs` (optional `docs` dependency group), published to GitHub Pages from `main` | Working decision |
| Documentation layout            | `docs/` holds the Architecture, Roadmap and Design pages (Design split by topic); `README.md`/`CONTRIBUTING.md` stay at the repo root | Working decision |
| C++ API reference (Doxygen)     | Not generated: the code is deliberately comment-light, so a generated reference would list signatures only, and Doxygen is a non-Python toolchain dependency. Revisit once the public C++ API stabilises | Deferred |
| Games/demos host                | `Oasis` (separate executable, links `Oryx`) | Established direction |
| Game/Strategy/Engine separation | Fundamental principle | Established           |
| Graphics                        | Separate from core    | Established direction |
| Headless operation              | First-class           | Established direction |
| Strategy observability          | Optional              | Established direction |
| Interface dispatch              | Virtual interfaces (`IGame`/`IState`/`IStrategy`, pure-virtual) | Working decision |
| State representation            | Mutable, in-place `apply()`/`undo()` | Working decision |
| Action representation           | Opaque `ActionId` (integer alias) | Working decision |
| Outcome/result model            | `Outcome` with templated `Rewards<T>` (runtime-sized per player) | Working decision |
| Player/turn model (Phase 2/3)   | Strict alternating turns only | Working decision (scoped) |
| Randomness utility              | `oryx::Random` (Core, seedable); not wired into chance nodes | Working decision |
| Phase 2 execution loop          | No `Simulation`/`Match`/`Runner` class; proven via test/demo loop | Working decision |
| Math module                     | Header-only `oryx::Math`: `oryx::math` `<cmath>` wrappers + constants, generic `Vector<N,T>` (+ `Vec2`/`Vec3`/`Vec4` headers), `Matrix<R,C,T>` (+ `Mat2`/`Mat3`/`Mat4`, bounded 2x2/3x3 determinant/inverse, 2D affine transform helpers), `Colour` — widened during Phase 2 planning from the original `Vec2`-only scope, then again for a general vector/matrix/colour pass | Working decision |
| Extension registration          | Self-registering factories, no central list; generic `Registry<T>` built in Phase 4, covering `IGame` and `IStrategy`, registered via `OX_REGISTER_GAME`/`OX_REGISTER_STRATEGY` macros expanding to a static self-registering object per type. Phase 7 adds construction parameters and per-entry metadata (see "Registry construction parameters" below) | Working decision |
| Naming conventions              | `snake_case` functions, `PascalCase` classes, `I`-prefix for pure interfaces, data-only structs | Working decision |
| Board rendering abstraction     | Concrete `TicTacToeBoard` class (Phase 3); `IBoard` deferred to Phase 10 | Working decision (scoped) |
| Application layering            | `Layer`/`LayerStack` owned by `Application` ([Architecture §3.6](../architecture.md#36-application-layers)); `LayerStack` constructs layers via `push_layer<T>()`/`push_overlay<T>()`; `run()` drives `update()`, events propagate top-down via `Layer::event()` until handled | Working decision |
| Test tiers beyond Unit          | Integration tier established (`tests/integration/`, same `Tests` binary/doctest, no new premake project); per-game (e.g. Tic-Tac-Toe) unit tests still deferred — "play it" remains sufficient for now | Working decision (scoped) |
| Simulation model (batched/eval) | `Match` (game + 2 strategies → `Outcome`) and a batch runner (N matches → aggregated win/loss/draw + `Rewards<T>`) in new `Oryx/Simulation` module; `SimulationLayer` (Layer subclass, defined in Oryx core) drives batches via `Application`'s tick loop | Working decision |
| Strategy location                | `RandomStrategy`/`FirstLegalStrategy`/`MinimaxStrategy` in `Oryx/Strategy` (game-agnostic); TicTacToe heuristic strategy in `Oasis` (game-specific), same reasoning as `TicTacToeBoard` | Working decision (scoped) |
| Strategy registry scoping        | Flat `Registry<IStrategy>` with a namespaced-name convention for game-specific strategies (e.g. `"tictactoe/heuristic"` vs. `"random"`); no compatibility enforcement this phase | Working decision (scoped) |
| `IState` clone/copy              | Not needed; `MinimaxStrategy` uses `apply()`/`undo()` depth-first on the same state object, no clone/copy-construction contract added | Working decision |
| Strategy randomness wiring       | No `IStrategy` interface change; `RandomStrategy` owns its own `oryx::Random`, seeded via constructor; batch-level seeding is the `Match`/runner's responsibility | Working decision |
| `SimulationLayer` location       | Defined in Oryx core (`Oryx/Simulation`) — first concrete `Layer` outside an app, since simulation-driving is Engine responsibility, not `Oasis`-specific | Working decision |
| Strategy/Game coupling          | `IStrategy::decide(const Context&)` replaces `decide(const IState&)`; `Context` (`Oryx/Game/Context.h`) wraps `IState&` plus capabilities explicitly attached via `provide<T>()`/`get<T>()` (keyed by `std::type_index`), not `dynamic_cast`-discovered from `State`'s class hierarchy — a capability can come from the game, the engine, or elsewhere. Built ahead of Phase 5 as forward-looking infrastructure (no `Engine`/`Match` yet — `Context` is currently constructed directly in `OasisLayer::attach()`/`update()`); no concrete capability shipped yet, since `RandomStrategy` already covers its own RNG need without one | Working decision |
| Parallelism model               | Explicitly deferred — Phase 5's batch runner is single-threaded by design; batching proves the API shape, not throughput | Deferred (explicit) |
| Serialization                   | Not decided           | Open                  |
| Profiling utility                | `oryx::Instrumentation`/`ScopeTimer` (`Oryx/Debug/Instrumentation.h`) — opt-in wall-clock timing; `OX_PROFILE_SCOPE` compiles to nothing unless `OX_ENABLE_PROFILING` is defined (Debug/Release only, stripped in `Dist`, same mechanism as `OX_ENABLE_ASSERTS`). `ProfileSample` now also tracks per-sample min/max, not just total/count. Instrumented call sites cover `TicTacToeState::legal_actions()` and each concrete strategy's `decide()` (`Minimax`/`Random`/`FirstLegal`/`TicTacToeHeuristic`), not just the test-only `DummyGame` | Working decision (scoped) |
| Strategy-vs-strategy benchmarking | No new engine abstraction: `Match`/`BatchRunner` already take a `SmallVector<IStrategy*, 2>` regardless of whether a seat is an AI strategy or `ExternalStrategy` (human input) — "player vs strategy"/"player vs player"/"strategy vs strategy" are all the same code path. New `Oryx/Benchmark` module (`Timer`, `BenchmarkRunner : public BatchRunner` with a nested `Results` type, `format_benchmark_report()`) adds timing/throughput on top, reported via a `--benchmark` flag on both the Oasis binary and `build run` (`build_system/commands/build.py`) — opt-in, default `--simulate` output is unchanged. The report keeps outcome quality (wins, rewards) and performance (throughput, memory, profile) in separate sections. A separate `build_system` benchmark command group was dropped: `build run --benchmark` covers it, so `build benchmark` is no longer a planned command | Working decision (scoped) |
| Event system adoption            | `Layer`/`EventDispatcher`/`Application::post_event()` existed but were unused in production code until now (only exercised by `tests/unit/Core/test_event.cpp`). First real usage: `StartSimulationEvent`/`SimulationCompleteEvent` (`Oryx/Events/SimulationEvent.h`) replace direct `push_layer<SimulationLayer>()`/`OX_CORE_INFO()`/`Application::Get().close()` calls between `OasisLayer`, `OasisApp`, and `SimulationLayer`. Required adding `Application::on_event()` (called from `post_event()` before the layer-propagation loop) since `Application` itself isn't a `Layer` and had no way to react to a posted event. `SimulationLayer`'s constructor now takes only `bool benchmark` — game/strategies/match_count/on_turn arrive later via `StartSimulationEvent`, consumed once the layer is in the stack and receives the same event through normal top-down propagation | Working decision |
| `IState::legal_actions()` allocation | Implemented ([Performance](quality.md#performance)): `SmallVector<ActionId, kActionListInlineCapacity>` (`ActionList`, capacity 9) replaces `std::vector` as the return type; `legal_actions()`'s share of `MinimaxStrategy::decide()` wall time measured dropping from ~24% to ~6.2%. Capacity is a single engine-wide ceiling (the return type of a pure-virtual method) measured from TicTacToe's board (9); a future game with a larger branching factor loses the inline fast path silently (still correct - `SmallVector` spills to heap) - re-measure, don't guess, when one is added. `TicTacToeState::legal_actions()` carries an `OX_CORE_ASSERT` tripwire against the capacity, so a board-shape change that breaks the measured bound fails loudly instead of silently regressing | Resolved (capacity scoped to games measured so far, guarded by assert) |
| SBO container reuse (`Rewards<T>`, strategy vectors, `Registry<T>`) | `Rewards<T>` (separately audited-hot, [Performance](quality.md#performance)) and `Match`/`BatchRunner`/`SimulationLayer`'s strategy vectors/`BatchResult::wins`/`IActionFeatures::decode()` (sized by `num_players()`, unaudited) moved to `SmallVector`; `Registry<T>` moved from `std::unordered_map` to a new open-addressing `FlatHashMap<Key,Value,N>` + `Pair<K,V>` (unaudited, Cold path) — all user-directed extrapolations beyond the [Performance](quality.md#performance) evidence | Working decision (unaudited beyond `legal_actions()`) |
| Runtime-configurable-capacity SBO container | Discussed alongside `SmallVector<T,N>`; deferred since no current caller needs a non-compile-time inline capacity. Re-checked (2026-09 follow-up audit) against a possible pybind11 binding (Phase 7 - calls the same `Registry<T>::register_factory()` as any C++ caller, no new code path) and concurrent registration (no threading model exists yet, [Parallelism](quality.md#parallelism)) - neither adds a caller. Still no runtime-configurable container needed | Open (re-affirmed) |
| `Registry<T>`/`FlatHashMap`/`SmallVector` thread-safety | None of the three synchronize; registration is expected single-threaded (self-registering static init). Documented in-code rather than enforced, since no concurrent registration path exists yet ([Parallelism](quality.md#parallelism) - multi-threaded simulation explicitly deferred) | Working decision (documented, unenforced) |
| `FlatHashMap` erase() | Not implemented, and Phase 7's first steps do not need it: replacing an entry re-registered under the same name is `insert_or_assign`. It becomes necessary for script reload (dropping entries a reloaded script no longer defines) and for releasing Python-held factories before interpreter finalisation, so it is deferred to that work rather than to Phase 7 as a whole | Open (deferred to script reload/shutdown) |
| Memory measurement | `oryx::MemoryTracker` (`Oryx/Debug/MemoryTracker.h`) replaces the global `operator new`/`delete` (every form, including aligned) and counts allocations, bytes, live and peak live bytes with relaxed atomics; a 16-byte header stores each block's size so `delete` needs no platform `malloc_usable_size`. The hook is compiled into every configuration (including `Dist`) because strategy benchmarking must always be able to report memory; the cost is a header plus a few relaxed atomics per allocation, and there is no opt-out, so it does not coexist with ASan's own `operator new` interposition. Memory reporting is split in two: `MemoryBenchmarkRunner : BenchmarkRunner` (`MemoryResults` extends `BenchmarkRunner::Results` with a `MemoryStats`; `SimulationCompleteEvent` carries `MemoryResults`) is always memory-aware and `BenchmarkRunner` is timing/throughput only; per-scope allocation attribution in `ScopeTimer`/`ProfileSample` is the profiling side, gated by `OX_ENABLE_MEMORY_TRACKING` (Debug/Release, stripped in `Dist`) on top of `OX_ENABLE_PROFILING`. Measure-only: no allocator-aware containers or arenas were introduced | Working decision (scoped) |
| Arena/allocator seam | Not introduced. The warm-path traffic that motivated it (`Match`/`Context`/`ActionHistory`, ~20 allocations per match) was removed with `SmallVector` instead (~2 per match now), so the remaining case for an arena is a search algorithm with per-node allocation (MCTS); the tracker is the evidence source | Open |
| `ActionHistory` shape | One `SmallVector<ActionId, kActionHistoryInlineCapacity = 16>` plus a cursor (undo/redo move the cursor; `record()` truncates the redo tail) instead of two stacks. `actions()` returns a `std::span` of the applied actions; `undo()`/`redo()` return `INVALID_ACTION` when there is nothing to do, and `Match::undo()`/`redo()` then leave the state untouched (previously `back()` on an empty vector). Capacity 16 sits above Tic-Tac-Toe's 9 plies (the longest game there is, pinned by a `static_assert` and an allocation check in `tests/unit/Oasis/test_tictactoe.cpp`); longer games spill to the heap - re-size when a longer game is added | Working decision |
| `I`-prefix rule | Amended ([Naming Conventions](cpp-api.md#naming-conventions)) rather than renaming: an interface has no data and only pure methods plus optional defaulted capability hooks (`IGame::action_features()`, `IStrategy::required_capabilities()`). The prior wording ("100% pure virtual, no concrete methods") contradicted the types [Naming Conventions](cpp-api.md#naming-conventions) itself named as examples | Resolved |
| Platform support | macOS and Linux build and pass. Linux: the `#error` in `PlatformDetection.h` was removed and `linkOryxWholeArchive()`'s Linux branch now passes an absolute archive path plus `pthread`; verified by running the CI steps (`build build all`, then the `--benchmark` smoke run) in an `ubuntu:24.04` x86_64 container with GCC 13.3 (under QEMU emulation), where the full test suite passed and the memory tracker reported the same ~2 allocations/match as macOS. Windows still `#error`s and was dropped from the CI matrix. Known rough edges, not fixed: `build_system/config.py` maps any aarch64 host to the `ARM64` platform, but `premake5.lua` only defines `x64` off macOS, and the pinned premake download is x86_64-only, so arm64 Linux hosts don't work; and `prune_stale_object_dirs` prints harmless "Skipping ...make: No rule to make target" warnings on a first build | Resolved for macOS/Linux x86_64; Windows unsupported |
| Oasis rules under test | `Tests` compiles Oasis's `TicTacToeGame.cpp` and `TicTacToeHeuristicStrategy.cpp` directly (not the executable's app/UI code), so game rules and the heuristic strategy are unit-tested. Consequence: `tictactoe`/`tictactoe/heuristic` now register inside the Tests binary, and the old "Oasis is not linked" guard became a positive registration check. A separate `OasisTests` binary was rejected because `build_system` supports one test suite | Working decision |
| `TicTacToeState::is_terminal()` | Scans the board for an empty cell instead of building a full `ActionList` (which also ran a profile scope) just to test emptiness; `outcome()` now computes `winner()` once. Measured with `--simulate=minimax,minimax,1 --benchmark` in Release (deterministic; profiler overhead included): ~39.5 ms before, ~24.8 ms after (three and five runs respectively, each within ~1 ms) | Working decision (measured) |
| `Instrumentation::results()` | Returns a `const&` to the registry instead of a copy of the whole map. The by-value form made `results().at(name)` bind a reference into a destroyed temporary (a bug in the first draft of `test_instrumentation.cpp`). The reference stays valid, but its contents change on `record()`/`reset()`, so callers that keep data across those calls copy the `ProfileSample` | Working decision |
| Standard includes live in `oxpch.h` | Oryx headers deliberately do not carry their own `<vector>`/`<typeindex>`/`<span>`/… includes; the precompiled header (`Oryx/src/oxpch.h`) is the one place ambient standard headers are declared, so a new std dependency is added there. A short-lived attempt to make every header compile standalone (plus a `build check headers` command) was reverted for this reason. Separately, `Application.h` no longer forward-declares `int main(int, char**)` (it blocked a plain `int main()` and served no purpose). `SharedPtr`/`create_shared` stay in `Base.h` although unused today | Working decision |
| Game-specific strategy lookup | `Registry<IStrategy>` keeps its flat namespaced-name convention (`"tictactoe/heuristic"`); a nicer per-game lookup API was raised but not specified | Open (deferred, unchanged from prior scoping) |
| Public API vs private backends | `Oryx/src` is the public API, the only include path consumers (`Oasis`, `tests`, third parties) get. `Oryx/backends/<Name>/` is private: optional or platform-specific implementation code compiled into `Oryx` by Premake (a build option for optional ones such as `Python`, an OS filter for platform ones such as a future `MacOS`, later Metal/GLFW for graphics). Public headers never include from `backends/`; backends implement interfaces declared in `Oryx/src`. Every vendored library, backend or not, lives in `Oryx/vendor`. The folder does not exist yet: it is created with the first backend (Phase 7, Python) | Working decision |
| Python build and hosts | Python is on by default and opt-out at build time (`--no-python` / `python = false`); only `Oryx/backends/Python/` needs pybind11 and `<Python.h>`, and CI gets a Python-off leg. Both hosts are supported from one binding source, in stages: embedding in Oasis first, the research host (`import oryx`) last; pybind11 is built by Premake and vendored at `Oryx/vendor/pybind11`; the interpreter's paths come from `sysconfig` through `--python-*` Premake options. Details: [Python API](python-api.md#hosts-and-build) | Working decision (staged) |
| Python class roles | Public ABCs `Game`/`State`/`Strategy`; private wrappers `PyGame`/`PyState`/`PyStrategy`; script-backed C++ adapters `PyScripted*` implementing language-agnostic `IScriptedGame`/`IScriptedState`/`IScriptedStrategy` in `Scripting/` | Working decision |
| Registry construction parameters | `Registry<T>::create(name, Params)`; `Params` is a string-keyed map of `bool`/`int64`/`double`/`string`, ids unversioned. Each registration declares a schema (keys, types, defaults); the registry stores it with a description as the entry's `EntryInfo`, and a bad key or type throws a `ParamError` naming the key. Amended from the brainstorm: `EntryInfo` holds only the schema and description, not an origin, so the core `Registry<T>` stays free of scripting concepts | Working decision |
| Origin-tagged registration | Same-origin re-registration replaces an entry (notebook-safe), and a clash across origins is an error unless `overwrite=True`. Built in a scripting-specific registry over `Registry<IGame>`/`Registry<IStrategy>` together with the first script-defined registrant, not in core `Registry<T>` | Working decision (deferred to script-backed types) |
| Script registration and discovery | Unity-style `class Nim(oryx.Game, id="nim")` with typed fields as the schema, plus `register_game` for factories. Sources are unioned from `--script`/`--module`, `ORYX_SCRIPT_PATH`, a zero-config scan for each runtime's file pattern and (Python only) entry points; C++ never parses TOML | Working decision |
| Scripting seam built early | `Oryx/src/Oryx/Scripting/` (`IScriptRuntime`, `IScripted*`, `ScriptSource`/`ScriptOrigin`, `discover_scripts()`, `ScriptError`, `ScriptRuntimeRegistry`, `ScriptingLayer`) is always compiled and language-agnostic. Built before a second implementation exists, **by choice**, contrary to the `IBoard`/`Registry<T>` precedent: it keeps public headers and applications free of language names and confines the Python switch to the backend | Working decision (deliberate exception) |
| Script runtime lookup | `PythonRuntime` self-registers as `"python"` via `OX_REGISTER_SCRIPT_RUNTIME`; the public `ScriptingLayer` creates one runtime per discovered language from the registry. No `PythonScriptingLayer` and no Python define in public headers; Oasis names no Python type | Working decision |
| `ScriptingLayer` in core | Second concrete `Layer` defined in Oryx core, justified like `SimulationLayer`: any application linking Oryx can embed scripting ([Architecture §3.6](../architecture.md#36-application-layers)) | Working decision |
| Python scope choices | TicTacToe and its heuristic stay in `Oasis`; numpy is optional (`oryx[numpy]`); `oryx.log` and `oryx.assertions` are Tier 1 modules | Working decision |
| Python config, seeding, extension module | Config keys `[python] enabled` (build option) and `[scripting] paths` (run-time). `simulate(seed=S)` gives strategies created by name that declare a `seed` parameter `S + seat_index`, with no `IStrategy`/`BatchRunner` change. The `_oryx` shared library for `import oryx` is built last and is **owned by Oasis**; the earlier `OryxPy` project is dropped | Working decision (layout open until the research stage) |
| Research and prototyping | First-class requirement: frictionless `import oryx`, notebook-safe registration, interactive `Match` stepping, `simulate()`, results as data, interruptible long runs, introspection, a prototype-to-C++ graduation path with differential testing. Full `Experiment` stays Phase 8 | Working decision |
| Python ownership and lifetime | Scoped to scripting: adapters hold `py::object` and forward under the GIL; a borrowed `IState&`/`Context` never outlives `decide()`; Python-held factories are released before interpreter finalisation; a script exception becomes a `ScriptError` that aborts that match, never Oasis. Pure-Python-host ownership of C++ objects and finalisation ordering stay open ([Architecture §14](../architecture.md#14-current-architectural-unknowns)) | Working decision (scoped to scripting) |
| Error system | Inner code throws `oryx::Error` (thin per-module subclasses: `ParamError`, `ScriptError`) and never catches; a few layer boundaries catch: `LayerStack` guards a layer's `attach`/`update`/`event`/`detach`, logs once via an explicit `err.log()` and disables the layer. `category()` is a virtual overridden per subclass. Nothing logs on construction. Applies to new code; existing error sites are not retrofitted. See [Error Handling](cpp-api.md#error-handling) | Working decision |
| Initialisation | `oryx::init()` (`Application.h`) initialises logging and is idempotent; hosts call it before anything logs: `EntryPoint.h`'s `main()`, the test `main()`, and `oryx.init()` in notebooks and the REPL (research host). `oryx::is_initialised()` reads a global flag. There is no standard-error fallback in `Error::log()`. Script-facing entry points that would otherwise hit a null logger are guarded by `OX_GUARDED_FUNC` (`Scripting/InitGuard.h`): `oryx.log.*` and `oryx.assertions.check` now, `make_game`/`make_strategy` later; the guard raises, it does not lazy-init. See [Python API](python-api.md) | Working decision |
| Language-agnostic script pieces | Log routing (`script_log`), checks (`script_check`, throwing `AssertionError`) and the init guard live in `Scripting/`, not in the Python backend, so another runtime gets them unchanged; the backend only binds them. `AssertionError` is in Core because a failed assertion is not a scripting concept | Working decision |
| Assertion hook | A replaceable handler in `Core/Assert.h` (default: log and trap). Only the Python-as-host module installs a throwing one; embedded Oasis keeps trapping. Built with the research host because the embedded host has no user for it | Working decision (built later) |

**Warm-path fix (2026-09, implemented).** `Context`'s capability storage moved
from `std::unordered_map<std::type_index, void*>` to
`SmallVector<Pair<std::type_index, void*>, 4>` (linear scan; capability sets are
tiny, and unlike `FlatHashMap` it is movable, which `Match::build_context`'s
return-by-value needs), and `ActionHistory`'s two `std::vector`s became one
`SmallVector<ActionId, 16>` plus an undo/redo cursor. Measured through
`--benchmark` on Tic-Tac-Toe batches: ~20.4 allocations/match before, 2.005 after
(the game state, plus the heap-allocated `Match` in the layer-driven path); a
`BatchRunner`-driven `Match` allocates only its state, locked in by
`tests/unit/Debug/test_memory_tracker.cpp`. The capability vectors
(`required_capabilities()`/`missing_capabilities()`) stay `std::vector`: an empty
one never allocates, and no built-in strategy declares capabilities.

**Measured follow-up (2026-09, `MemoryTracker`).** The audit above inferred
allocation cost from wall time. `oryx::MemoryTracker`
(`Oryx/Debug/MemoryTracker.h`) now counts allocations directly, and the numbers
are deterministic enough to assert in the unit suite
(`tests/unit/Debug/test_memory_tracker.cpp`):

* `DummyState::legal_actions()`: 0 allocations across 144,664 calls in one full
  Minimax search; a two-player `Rewards<double>` construct/copy: 0 allocations.
* `MinimaxStrategy::decide()`: 0 allocations once the profiler registry is warm.
* `ScopeTimer` previously took its name as a `std::string` by value; the scope
  names exceed SSO, so the profiler itself heap-allocated on every scope and the
  earlier wall-time shares included that cost. It now stores a `const char*`
  (string literals only, enforced by `OX_PROFILE_SCOPE`'s contract), which is
  what makes per-scope allocation counts meaningful.
* Every profiled strategy/`legal_actions()` scope reports 0 allocs/call, yet a
  full Tic-Tac-Toe match costs ~20 allocations (~2.9 per decision), identical in
  Debug and Release. That traffic is in `Match`/`Context`/`ActionHistory`
  (warm path), not in any strategy. Which of those is worth changing is left to
  the pre-Phase-7 audit, not decided here.

The ~280 bytes / 5 allocations of `Instrumentation`'s registry (first insert per
scope name) fall inside the measured window, so very short runs slightly inflate
per-match figures.

## History: Architecture Phase

The next major design activity should be a dedicated architecture and design brainstorming session.

That session should investigate, among other things:

* Core domain model
* Game lifecycle
* State and action representation
* Player/agent model
* Turn and chance models
* Information models
* Strategy API
* Simulation orchestration
* Evaluation
* Experiment model
* Randomness
* Reproducibility
* Observability
* Python API
* Extension mechanisms
* Serialization
* Parallel execution
* Graphics boundaries

The purpose is not to design every future feature.

The purpose is to identify the **smallest coherent core** from which those features can grow.
