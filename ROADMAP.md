# Oryx Roadmap

> **Status: Preliminary — direction rather than a fixed schedule**

This roadmap describes the intended evolution of Oryx.

The roadmap deliberately avoids assigning dates to features that depend on architectural decisions that have not yet been made.

The immediate priority is to establish a small, coherent foundation before expanding into algorithms, games, visualisation, and research tooling.

---

# 1. Development Philosophy

Oryx should evolve incrementally.

The preferred progression is:

```text
Architecture & Design
    ↓
Minimal Build System
    ↓
Minimal C++ Core
    ↓
First Reference Game
    ↓
Baseline Strategies
    ↓
Simulation & Evaluation
    ↓
Benchmarking
    ↓
Python Research Layer
    ↓
Experiment Framework
    ↓
Strategy Observability
    ↓
Strategy Dashboard
    ↓
Graphics & Visualisation
    ↓
Larger Algorithm Ecosystem
    ↓
Larger Game Library
    ↓
Ecosystem & Extensions
```

Each stage should provide a useful, testable foundation for the next.

---

# 2. Phase 0 — Architecture & Design

**Status: Complete**

Before implementing the substantial engine, conduct a dedicated architecture and design brainstorm.

### Goals

Define the smallest useful conceptual core.

### Questions

* What exactly is a Game?
* What exactly is a State?
* What exactly is an Action?
* What exactly is a Strategy?
* How are players represented?
* How are turns represented?
* How is chance represented?
* How are terminal outcomes represented?
* How are games executed?
* How are simulations represented?
* How are results represented?
* How should randomness work?
* How should observability work?
* What belongs in the engine?
* What belongs outside it?
* What should the Python API feel like?

### Deliverable

A reviewed architecture and initial API design.

**Delivered:** see `ARCHITECTURE.md` and `DESIGN.md`, which capture the
resolved core model (`IGame`/`IState`/`IStrategy`, `ActionId`,
`Outcome`/`Rewards<T>`, naming conventions, extension/registration approach)
and the decision log (`DESIGN.md` §19) recording what's settled versus still
open for later phases.

---

# 3. Phase 1 — Minimal Build System

Establish the project's foundational development and build workflow before implementing the C++ engine.

The build system is implemented as the **`build_system` Python module**.

It provides a Python-based developer CLI around Premake5 and the project's development workflows.

### Initial Goals

* Establish the Python package/module structure
* Provide the `build` CLI
* Integrate Premake5
* Support local Premake5 dependency management
* Support Debug, Release, and Distribution configurations
* Configure C++ project generation
* Compile the project
* Run tests
* Clean generated artifacts
* Provide a complete configure → compile → test workflow
* Establish a foundation for future development commands, via a decorator-based
  command registry (`build_system/registry.py`) that auto-discovers new
  `commands/<name>.py` modules and their interactive-menu entries
* Scaffold `Oasis`, the companion executable that links `Oryx`, as the home for games/demos outside the engine core
* Generate optional, per-developer IDE integration (`build config init --ide vscode|visual_studio`) —
  VS Code tasks/launch configs with single-button build+debug per profile, or a generated Visual Studio solution
* Provide a generic `vendor/<lib>` convention (Premake's `useVendorHeader()` + `build_system/vendor.py`)
  for vendoring header-only third-party libraries as git submodules, under `Oryx/`, `Oasis/`, and `tests/`

### Initial Commands

The exact CLI may evolve, but the initial workflow should support concepts such as:

```text
build config init
build build configure
build build compile
build build clean
build build run
build test run
build build all
```

The build system should remain a **thin developer tooling layer** around Premake5 rather than becoming a second build system.

### Future Extensions

The module may eventually provide commands such as:

```text
build benchmark
build experiment
build docs
build explain
```

These should be introduced when the corresponding project capabilities exist.

### Deliverable

A reproducible development workflow capable of configuring, building, testing, and cleaning the initial C++ project.

---

# 4. Phase 2 — Minimal C++ Core

Build the smallest functional engine, using the interfaces decided during
the Phase 0 brainstorm (see `ARCHITECTURE.md` §3 and `DESIGN.md` §19 for
full detail).

### Components

* `IGame` — pure-virtual, stateless factory (`new_initial_state()`, static
  game info)
* `IState` — pure-virtual; owns all rules and behaviour: `legal_actions()`,
  `apply(ActionId)`/`undo(ActionId)` (mutated in place), `current_player()`,
  `is_terminal()`, `outcome()`, `action_to_string(ActionId)`
* `ActionId` — opaque integer alias, game-defined meaning
* `Outcome` — `{ is_terminal, Rewards<double> }`; `Rewards<T>` is templated
  on reward type, runtime-sized per the game's player count
* `IStrategy` — pure-virtual, conceptually `ActionId decide(const IState&)`;
  implementations may hold internal state across calls
* `oryx::Random` — standalone, seedable utility in `Oryx/Core`; not yet
  wired into `IGame`/`IState` (no chance nodes in Phase 2/3)
* `oryx::Math` — header-only module (`oryx::math` `<cmath>` wrappers +
  constants, generic `Vector<N,T>`/`Matrix<R,C,T>` with `Vec2/3/4` and
  `Mat2/3/4` aliases, bounded 2x2/3x3 determinant/inverse, 2D affine
  transform helpers, a minimal `Colour`), included via `oxpch.h`, scoped
  to grid/board coordinate and 2D-transform needs (see `ARCHITECTURE.md`
  §3.4)
* Unit testing infrastructure (doctest), validating the above against a
  minimal/dummy game — not Tic-Tac-Toe itself, which is Phase 3's deliverable

### Explicitly deferred

* No `Simulation`/`Match`/`Runner` class yet — the execution loop (legal
  actions → strategy decides → apply → check terminal → repeat) is proven
  via tests/a demo loop, not a dedicated abstraction (`ARCHITECTURE.md` §5)
* No `Registry<T>` — games/strategies will self-register (the principle is
  decided, `ARCHITECTURE.md` §10), but the mechanism isn't built until a
  second game/strategy makes manual construction inconvenient
* No chance/simultaneous player support — strict alternating turns only
* No concrete `Layer` implementations beyond `Oasis`'s own — `Application`
  gained a `LayerStack`/`Layer` extension point (`ARCHITECTURE.md` §3.6), but
  `SimulationLayer`, a `PythonScriptingLayer`, a GUI/CLI layer, and
  profiling/benchmarking layers are future direction only, tied to Phases
  6/7/9/11 below

---

# 5. Phase 3 — First Reference Game

Implement a deliberately simple reference game.

A game such as Tic-Tac-Toe is useful because it can exercise:

* State representation
* Action generation
* Turn handling
* Terminal states
* Outcomes
* Simple strategies
* Deterministic testing

The purpose is architectural validation, not creating a large game library immediately.

`Oasis`, the companion executable that links against `Oryx`, is the intended
host for this reference game and later games/demos — it already exists as a
scaffold (Phase 1) ahead of this phase's actual content.

### Concrete near-term milestone

A terminal-playable Tic-Tac-Toe: two players alternate entering moves via
stdin, the board prints to stdout after each move, and the game reports the
outcome once terminal — no `Graphics` system involved (that's Phase 11).

This needs a `TicTacToeBoard` component (in `Oasis`) responsible for both
rendering the board to stdout and reading a move from stdin. It is a
**concrete class, not an interface** — with only one game and one renderer,
a shared `IBoard` interface has no second implementation to justify it yet
(same reasoning as the `Registry<T>` timing decision in Phase 2). `IBoard`
gets extracted once Phase 11 Graphics actually needs to swap in a graphical
renderer polymorphically, per `ARCHITECTURE.md` §8.

---

# 6. Phase 4 — Baseline Strategies

Implement simple strategies to validate the Strategy abstraction.

Potential initial strategies:

* Random
* First legal action
* Simple heuristic
* Minimax

These should provide progressively stronger validation of the strategy interface.

The goal is to test whether the architecture works across different decision-making styles.

### Concrete near-term milestone

Four strategies, confirmed rather than merely potential:

* `RandomStrategy`, `FirstLegalStrategy`, `MinimaxStrategy` — game-agnostic,
  depending only on `IState`, living in `Oryx/src/Oryx/Strategy/`
* A TicTacToe-specific heuristic strategy — living in `Oasis`, for the same
  reason `TicTacToeBoard` stayed concrete/local rather than becoming a core
  abstraction (`ARCHITECTURE.md` §8)

Minimax exercises lookahead using the existing `apply()`/`undo()` contract
directly — depth-first search on the same state object, undoing after each
branch. No `clone()`/copy-construction is added to `IState` for this.

This phase also builds `Registry<T>`, generic and wired up for both `IGame`
and `IStrategy` from the start (see `ARCHITECTURE.md` §10 and `DESIGN.md`
§19). Games and strategies register via `OX_REGISTER_GAME`/
`OX_REGISTER_STRATEGY` macros that expand to a self-registering static
object per type — no central list, no `__init__.py`-style registration
file to maintain. Game-specific strategies (like the heuristic above)
register under a namespaced name (e.g. `"tictactoe/heuristic"`) rather than
a global one (`"random"`); the registry itself doesn't enforce game/strategy
compatibility yet.

---

# 7. Phase 5 — Simulation & Evaluation

Introduce reusable simulation infrastructure.

Potential capabilities:

* Run individual games
* Run batches of games
* Compete strategies
* Collect outcomes
* Aggregate statistics
* Configure seeds
* Reproduce experiments

The result should make it easy to answer questions such as:

```text
How does Strategy A perform against Strategy B
over 100,000 games?
```

### Concrete near-term milestone

A new `Oryx/src/Oryx/Simulation/` core module:

* `Match` — one game + two strategies → `Outcome`
* A batch runner — N repeated matches → aggregated win/loss/draw counts and
  aggregate `Rewards<T>`
* `SimulationLayer` — a concrete `Layer` subclass defined in Oryx core (not
  Oasis) that drives a batch through `Application`'s tick loop; `Oasis`
  pushes it onto its `LayerStack` the same way it pushes `OasisLayer` today
  (`ARCHITECTURE.md` §3.6)
* An action decode/interpretation capability (e.g. `IActionFeatures`),
  resolved through `Context` like any other capability — `ActionId` stays
  the opaque wire type (`ARCHITECTURE.md` §3.2/§14, `DESIGN.md` §19); this
  is the mechanism for a strategy that needs structured access to what an
  action means, not a reopening of the Action representation decision
* `Context`/capability construction moves from `OasisLayer` (a stopgap, see
  `ARCHITECTURE.md` §14) into `Match`, now that a real orchestration entry
  point exists

This phase is explicitly single-threaded — batching proves the `Match`/
runner API shape, not throughput. Parallel batch execution is deferred (see
`DESIGN.md` §13/§19); the "100,000 games" example above is a target for the
API to express cleanly, not a performance bar this phase needs to clear.

---

# 8. Phase 6 — Benchmarking

Introduce benchmark infrastructure for computational performance.

Potential metrics:

* Games per second
* Decisions per second
* Nodes explored
* Memory usage
* Search time
* Rollout throughput

Benchmarks should distinguish algorithmic performance from game outcome quality.

---

# 9. Phase 7 — Python Research Layer

Introduce the first useful Python API through pybind11.

Initial goals:

* Create games
* Create strategies
* Run simulations
* Collect results
* Configure experiments
* Access statistics

Python should provide a natural interface for experimentation rather than expose the entire C++ implementation.

---

# 10. Phase 8 — Experiment Framework

Build higher-level experimentation capabilities.

Potential features:

* Parameter sweeps
* Repeated trials
* Seed management
* Experiment configuration
* Result storage
* Statistical summaries
* Reproducibility metadata
* Comparative analysis

A central goal is making large experiments easy to describe and repeat.

---

# 11. Phase 9 — Strategy Observability

Introduce optional strategy instrumentation.

Potential capabilities:

* Decision traces
* Action probabilities
* Values
* Search statistics
* Search trees
* Simulation counts
* Convergence information
* Algorithm-specific metrics

The observability model should be extensible rather than forcing all algorithms into the same schema.

---

# 12. Phase 10 — Strategy Dashboard

Build a lightweight visual interface for inspecting algorithm behaviour.

Potential visualisations include:

* Current game state
* Selected action
* Action probabilities
* Value estimates
* Search trees
* Simulation statistics
* Decision traces
* Algorithm-specific diagnostics

The dashboard should consume observability data and remain separate from the strategy implementation.

---

# 13. Phase 11 — Graphics & Visualisation

Introduce graphics capabilities where they provide clear value.

Initial focus:

* Board games
* Grid games
* Card games
* Strategy games
* Simulation visualisation
* Algorithm visualisation

The graphics system should remain lightweight and specialised to Oryx's use cases.

---

# 14. Phase 12 — Algorithm Ecosystem

Expand the strategy and algorithm library.

Potential areas include:

### Search

* Minimax
* Alpha-Beta pruning
* Monte Carlo Tree Search
* Iterative deepening
* Transposition tables

### Optimisation

* Dynamic programming
* Local search
* Genetic algorithms
* Mathematical optimisation interfaces

### Probability & Simulation

* Monte Carlo methods
* Markov processes
* Stochastic decision models

### Learning

* Reinforcement learning
* Policy methods
* Value methods
* Self-play

### Game Theory

* Best-response methods
* Regret-based algorithms
* Equilibrium-related algorithms

Each addition should be justified by actual use cases and fit the core architecture.

---

# 15. Phase 13 — Larger Game Library

Only after the core architecture has demonstrated itself should Oryx expand substantially into games.

Potential categories:

* Board games
* Card games
* Grid games
* Abstract strategy games
* Stochastic games
* Imperfect-information games
* Educational environments

Games should ideally live outside the core engine where practical.

---

# 16. Phase 14 — Ecosystem & Extensions

Longer-term possibilities include:

* External game packages
* External strategy packages
* Plugin mechanisms
* Experiment packages
* Visualisation extensions
* Educational modules
* Community-contributed algorithms

The extension model should remain simple enough that contributors can understand it without learning a large framework.

---

# 17. Build & Developer Tooling

The **`build_system` Python module** should evolve alongside the project.

The intended development workflow is exposed through the `build` CLI.

For example:

```text
build config init
build build configure
build build compile
build build clean
build build run
build test run
build build all
```

As corresponding capabilities are implemented, the CLI may grow to support:

```text
build benchmark
build experiment
build docs
build explain
```

The build system should remain a thin developer-experience layer around Premake5 and should not duplicate the responsibilities of the underlying C++ build system.

New commands register via a small decorator (`build_system/registry.py`) rather
than hand-wired lists, so extending the CLI is a matter of adding a
`commands/<name>.py` module. Optional, per-developer IDE integration
(`build config init --ide vscode|visual_studio`) and a generic `vendor/<lib>`
convention for vendored header-only dependencies are part of this layer too —
see [`build_system/README.md`](build_system/README.md) for details.

---

# 18. Documentation & Education

Documentation should eventually cover:

* Getting started
* Architecture
* Game implementation
* Strategy implementation
* Simulation
* Experiments
* Algorithms
* Python API
* Visualisation
* Strategy observability

The `explain` workflow is intended to make algorithms approachable for learners as well as useful to experienced developers.

---

# 19. What We Should Explicitly Avoid

The roadmap does **not** currently prioritise:

* Becoming a general-purpose game engine
* Full 3D rendering
* A massive built-in game library
* A large framework of abstractions
* Complex plugin infrastructure before it is needed
* Premature distributed computing
* Premature GPU infrastructure
* Building every possible algorithm
* Features without a demonstrated use case

Oryx should remain a **Game Strategy Engine**, not gradually turn into an unrelated general-purpose engine.

---

# 20. Roadmap Principle

The roadmap is intentionally flexible.

A future feature should be evaluated against:

```text
Does it strengthen the Game Strategy Engine?
        │
        ├── Yes → investigate
        │
        └── No → probably keep it outside the core
```

The architecture should constrain the roadmap rather than the roadmap forcing complexity into the architecture.
