# Oryx Architecture

> **Status: Preliminary — subject to architecture and design review**

This document describes the current architectural model for Oryx, the Game Strategy Engine.

It intentionally establishes **boundaries and responsibilities** rather than prematurely defining every interface. The project is expected to undergo a dedicated architecture and design brainstorming phase before the core APIs are considered stable.

---

# 1. Architectural Goal

Oryx should provide a small and extensible foundation for:

* Defining games
* Defining strategies
* Executing games
* Running simulations
* Evaluating strategies
* Running experiments
* Analysing results
* Inspecting algorithm behaviour
* Visualising games and algorithms

The central architectural principle is:

```text
Game != Strategy != Engine
```

These responsibilities must remain independently understandable and replaceable.

---

# 2. High-Level Model

The current conceptual model is:

```text
                         ┌─────────────────────┐
                         │        Games        │
                         │                     │
                         │ Rules               │
                         │ State               │
                         │ Actions             │
                         │ Transitions         │
                         │ Outcomes            │
                         └──────────┬──────────┘
                                    │
                                    │ Game abstraction
                                    ▼
┌──────────────────┐      ┌─────────────────────┐
│    Strategies    │◄────►│       Engine        │
│                  │      │                     │
│ Decision making  │      │ Execution           │
│ Search           │      │ Simulation          │
│ Heuristics       │      │ Evaluation          │
│ Learning         │      │ Infrastructure      │
└──────────────────┘      └──────────┬──────────┘
                                     │
                   ┌─────────────────┼─────────────────┐
                   ▼                 ▼                 ▼
             Experiments        Observability      Results
                   │                 │                 │
                   └─────────────────┼─────────────────┘
                                     ▼
                            Python / Visualisation
```

This is a conceptual model, not yet a final module diagram.

---

# 3. Core Components

## 3.1 Game

`IGame` is a pure-virtual interface representing a particular game or
decision environment. It is deliberately thin: a stateless factory that
exposes static game information and produces states.

`IGame` is responsible for:

* Identifying the game (name, number of players)
* Producing a fresh initial state (`new_initial_state()`)

`IGame` should not:

* Hold or mutate game state itself
* Select actions on behalf of an agent
* Know which strategy is being used
* Depend on the visualisation system
* Depend on experiment infrastructure

All actual rules — legal actions, applying actions, terminal detection,
outcomes — live on `IState` (§3.2), not on `IGame`. A game should be usable
independently of graphics and the Strategy Dashboard.

See `DESIGN.md` §5 for the naming rationale (`I`-prefix reserved for fully
pure-virtual interfaces).

---

## 3.2 State

`IState` is a pure-virtual interface representing a specific position within
a game, and owns essentially all of the game's rules and behaviour:

* `legal_actions()` — the actions available to the current player
* `apply(ActionId)` — mutates the state in place
* `undo(ActionId)` — reverses the most recent `apply`, in place
* `current_player()` — whose turn it is
* `is_terminal()` — whether the game has ended
* `outcome()` — the result once terminal (see `Outcome` in §4)
* `action_to_string(ActionId)` — a human-readable action, for logging/debugging

State is mutated in place (`apply`/`undo`) rather than returned as a new
value. This matches how tree-search algorithms such as minimax and MCTS are
conventionally implemented: exploring a branch does not require copying the
entire state at every node, only applying and later undoing one action.

An action is represented by `ActionId`, an opaque integer alias, not a
polymorphic type — its meaning is entirely defined by the game that produced
it. This keeps the `IState` interface stable across every game and keeps
action generation (a hot path in search) allocation-free.

`IState` should not:

* Select actions on behalf of an agent
* Know which strategy is being used
* Depend on the visualisation system

---

## 3.3 Strategy

`IStrategy` is a pure-virtual interface representing decision-making,
conceptually `ActionId decide(const IState&)`. Unlike `IGame`/`IState`,
`IStrategy` implementations may hold internal mutable state across calls
(e.g. a persisted search tree, learned parameters) — statelessness is not
required.

Its fundamental responsibility is conceptually:

```text
State → Action
```

Depending on the algorithm, this may internally involve:

```text
State
 ↓
Search / Evaluation / Optimisation / Learning
 ↓
Decision
 ↓
Action
```

Strategies should consume game capabilities through stable abstractions.

Potential strategies include:

* Random
* Rule-based
* Heuristic
* Minimax
* Alpha-Beta
* Monte Carlo Tree Search
* Monte Carlo simulation
* Dynamic programming
* Reinforcement learning
* Game-theoretic algorithms

The architecture should avoid making assumptions that all strategies share the same internal algorithm.

---

## 3.4 Math

`Math` is a small, header-only module (`Oryx/src/Oryx/Math/`) providing
templated vector types for grid/board coordinates, e.g.
`template<typename T> struct Vec2 { T x, y; };` with numeric aliases
(`Vec2f`, `Vec2d`, `Vec2i`). Being header-only, it is included directly from
the precompiled header (`oxpch.h`).

Per the struct-is-data-only convention (`DESIGN.md` §5), `Vec2<T>` holds
only its fields — arithmetic and other operations are free functions, not
members.

`Math` is scoped to what board/grid games and, later, graphics actually
need. It is explicitly not a general-purpose maths library and should grow
only from demonstrated requirements (`DESIGN.md` §20).

---

## 3.5 Outcome

Once `IState::is_terminal()` is true, `IState::outcome()` returns an
`Outcome`: `{ bool is_terminal; Rewards<double> rewards; }`. `Rewards<T>` is
a templated class holding one reward per player, sized at runtime from the
game's player count (not templated on player count, since `IGame`/`IState`
are chosen at runtime through a virtual interface). This represents
2-player zero-sum outcomes today (Tic-Tac-Toe: +1/-1/0) without requiring a
breaking change when N-player or non-zero-sum games arrive later.

---

# 4. Engine

The Engine provides execution infrastructure.

Potential engine responsibilities include:

### Simulation

Running games between strategies or other agents.

### Evaluation

Measuring strategy performance across games or environments.

### Experimentation

Running controlled collections of simulations with configurable parameters.

### Randomness

Providing reproducible random-number generation and seed management. A
standalone, seedable `oryx::Random` utility (wrapping `std::mt19937_64`)
lives in `Oryx/Core`. It is not wired into `IGame`/`IState` — Phase 2/3 have
no chance nodes — but exists so Phase 4's Random strategy, and later
reproducibility work, have a single reproducible source.

### Statistics

Collecting metrics and producing structured results.

### Benchmarking

Measuring computational performance independently from game outcomes.

### Infrastructure

Providing reusable functionality required by games, strategies, simulations, and experiments.

The engine should not become a dumping ground for unrelated functionality.

---

# 5. Execution Model

A basic execution flow may eventually resemble:

```text
Create Game
    ↓
Create Strategy A
Create Strategy B
    ↓
Create Simulation / Match
    ↓
Initial State
    ↓
Strategy chooses Action
    ↓
Game applies Action
    ↓
New State
    ↓
Repeat
    ↓
Terminal State
    ↓
Outcome
    ↓
Evaluation / Statistics
```

The exact orchestration model remains an open design question.

In particular, we should determine whether a dedicated `Simulation`, `Match`, `Runner`, or similar abstraction is actually necessary before introducing one.

For Phase 2 specifically, no such abstraction is introduced yet. The loop
above is validated directly — via a doctest test exercising a minimal game,
and/or a small demonstration loop in `Oasis` — rather than through a
dedicated class. Introducing `Simulation`/`Match`/`Runner` remains a Phase 5
question once batched simulation and evaluation are actually needed.

---

# 6. Observability

Observability is an optional cross-cutting capability.

The core decision-making path should remain lightweight.

A strategy may optionally publish diagnostics such as:

```text
Decision
├── selected action
├── action probabilities
├── expected values
├── value estimates
├── search depth
├── simulations
├── nodes explored
├── search tree
├── constraints
├── regret
└── algorithm-specific metrics
```

The system should support both:

```text
Strategy
    ↓
Action
```

and, when requested:

```text
Strategy
    ↓
Decision + Diagnostics
    ↓
Action
```

The second path must not be mandatory for ordinary execution.

---

# 7. Strategy Dashboard

The Strategy Dashboard is intended to consume observability data rather than become part of strategy logic.

Conceptually:

```text
Strategy
    │
    ├── Decision
    │
    └── Optional Diagnostics
              │
              ▼
       Observability Layer
              │
              ▼
       Strategy Dashboard
```

This separation allows:

* Headless execution
* Automated experiments
* Different frontends
* Logging
* Debugging
* Educational visualisation

without coupling algorithms to a particular UI.

---

# 8. Graphics

Graphics are separate from the engine's core game logic.

Conceptually:

```text
Game State
    │
    ├──────────────► Simulation / Engine
    │
    └──────────────► Renderer / UI
```

The graphics system should eventually support the project's primary use cases:

* Board games
* Card games
* Grid games
* Strategy games
* Simulation visualisation
* Algorithm visualisation

It is explicitly **not** intended to become a general-purpose engine comparable to Unity or Unreal.

### Board (Phase 3)

The `Renderer / UI` role above starts, in Phase 3, as a concrete
`TicTacToeBoard` class in `Oasis` — responsible for both rendering the board
to stdout and reading a move from stdin. It is deliberately **not** behind a
shared `IBoard` interface yet: with only one game and one renderer, an
interface has no second implementation to justify it (the same reasoning as
the `Registry<T>` timing decision, §10). `IBoard` should be extracted once
Phase 11 Graphics actually needs to swap in a graphical renderer
polymorphically — not before.

---

# 9. Python Layer

Python acts as the research and experimentation layer.

A conceptual architecture is:

```text
                 Python
                   │
          ┌────────┴────────┐
          │                 │
      High-level API     Tooling
          │                 │
          ▼                 ▼
       pybind11         Experiments
          │             Analysis
          ▼             Visualisation
        C++ Core        Benchmarks
```

The Python API should expose concepts useful to researchers and users rather than mirroring internal C++ classes one-to-one.

For example, Python users should ideally be able to express experiments naturally without understanding the internal C++ ownership model.

---

# 10. Extension Model

The intended extension model is:

```text
             Oryx Core
                 │
       ┌─────────┼─────────┐
       ▼         ▼         ▼
     Games   Strategies   Tools
       │         │         │
       ▼         ▼         ▼
   Tic-Tac-Toe MCTS      Analysis
   Chess       Minimax   Visualisation
   Cards       RL        Experiments
```

Adding a new game should normally mean implementing the appropriate game abstractions.

Adding a new strategy should normally mean implementing the appropriate strategy interface.

Neither should require modifying the engine core.

The exact extension mechanism—static registration, factories, modules, plugins, or another approach—remains open.

### Registration

Games and strategies register themselves — there is no central file listing
every game or strategy (contrast with, e.g., Gymnasium's pattern of one
`register()` call per environment in a shared `__init__.py`). The intended
mechanism is a self-registering factory: a game or strategy registers a
name and a factory function via a static object in its own `.cpp` file, so
adding a new one never requires editing shared engine code.

This mirrors `build_system/registry.py`'s decorator-based command
auto-discovery already used in this project's Python tooling.

The actual `Registry<T>` utility is not built in Phase 2 — with only one
game (Tic-Tac-Toe, Phase 3) there is nothing yet to register. It should be
introduced whenever manual construction first becomes inconvenient, likely
around Phase 4's baseline strategies or Phase 13's larger game library.

### Oasis

`Oasis` is a companion executable project (`Oasis/`) that links against `Oryx`
the same way `tests/` does. It is where games, demos, and experiments that
consume the engine should live, so that this kind of content never needs to be
compiled into `Oryx` itself. It is currently a scaffold — the smallest possible
program proving the link works — and is expected to grow into the home for the
first reference game described in `ROADMAP.md` Phase 3.

---

# 11. Dependency Direction

The preferred dependency direction is:

```text
Game-specific code
        ↓
Core abstractions
        ↑
Strategies
        ↑
Engine infrastructure
```

More precisely, higher-level orchestration should depend on stable abstractions rather than concrete game implementations.

UI and tooling should consume engine/game/observability interfaces rather than becoming dependencies of the core.

This should be validated during the architecture phase.

---

# 12. Headless Architecture

Headless execution is a first-class requirement.

The following should be possible without graphics:

```text
Game
+
Strategy
+
Engine
+
Simulation
+
Statistics
```

This is particularly important for:

* CI
* Large simulations
* Research
* Server environments
* Reinforcement learning
* Benchmarking
* Automated experiments

---

# 13. Reproducibility

Where practical, execution should support deterministic reproduction.

Important considerations include:

* Explicit random seeds
* Controlled random-number generators
* Experiment configuration
* Versioned parameters
* Structured result output
* Environment information
* Algorithm configuration

Reproducibility requirements will be refined once the experiment model is designed.

---

# 14. Current Architectural Unknowns

The Phase 2 brainstorm (see `DESIGN.md` §19 decision log) resolved the
following, at least for the minimal core:

* Exact `Game`/`State` interface — resolved: `IGame` (stateless factory) /
  `IState` (rules, mutated in place via `apply`/`undo`)
* Exact `Strategy` interface — resolved: pure-virtual `IStrategy`, may hold
  internal state across calls
* State ownership and representation — resolved: mutable, in-place
* Action representation — resolved: opaque `ActionId` (integer alias)
* Turn management — resolved, minimally: strict alternating turns only
* Result model — resolved: `Outcome` with a templated `Rewards<T>` (§3.5)
* Randomness abstraction — partially resolved: a standalone `oryx::Random`
  utility exists, but is not wired into `IGame`/`IState`
* Plugin architecture — narrowed to **registry mechanism timing**: the
  self-registering-factory principle is decided (§10); only the timing of
  building the actual `Registry<T>` utility remains open

The following should **not** be considered settled yet:

* Player/agent model beyond strict alternation (chance players,
  simultaneous-move players)
* Simultaneous actions
* Chance/nature actions
* Imperfect information
* Game history
* Simulation orchestration
* Evaluation API
* Experiment representation
* Observability protocol
* Dashboard transport
* `Registry<T>` implementation and timing
* Python ownership/lifetime semantics
* Serialization
* Graphics abstraction
* Multi-threaded simulation model

These should be addressed systematically rather than solved piecemeal during implementation.

---

# 15. Architectural Principle

When in doubt:

> **Keep the core smaller.**

An abstraction should earn its place by solving a real recurring problem.

Oryx should favour composition, explicit interfaces, dependency inversion, testability, and extension points while avoiding framework-like complexity that is not justified by actual use cases.
