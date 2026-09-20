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

See [Design: Naming Conventions](design/cpp-api.md#naming-conventions) for the naming rationale (`I`-prefix reserved for fully
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

This was validated directly by Phase 4/5's `MinimaxStrategy`: full-tree
lookahead recurses depth-first on the same `IState` object via `apply()`/
`undo()`, exactly like every other consumer — no `clone()`/copy-construction
contract was added to `IState` ([Design: Decision Log](design/decision-log.md)).

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
`IStrategy::decide` takes a `Context` (`Oryx/Game/Context.h`), not `IState`
directly: `Context` wraps the `IState&` plus any capabilities the
orchestrator explicitly attaches via `provide<T>()`, retrieved via
`get<T>()`/`has_capability()`. A capability is not discovered via
`dynamic_cast` against the concrete `State` — it can come from the game, the
engine, or anywhere else the orchestrator chooses, which is also why
`RandomStrategy` still owns its own `oryx::Random` directly rather than
receiving it as a capability. `IStrategy::required_capabilities()` lets a
strategy declare what it needs so the orchestrator (currently
`OasisLayer::attach()`, no `Engine`/`Match` yet — see §14) can fail fast with
a clear diagnostic instead of a null-pointer dereference in search.

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

`RandomStrategy` (Phase 4) owns its own seedable `oryx::Random` member,
seeded via its own constructor argument — `IStrategy` itself gained no
seed/RNG parameter. Reproducibility/seed control for a batch is instead
orchestrated at the `Match`/batch-runner level (`Oryx/Simulation`, §5),
which decides how to construct and seed strategies for a run ([Design: Randomness](design/determinism.md#randomness)/[Decision Log](design/decision-log.md)).

---

## 3.4 Math

`Math` is a header-only module (`Oryx/src/Oryx/Math/`), widened during
Phase 2 planning beyond the narrower "`Vec2` struct" scoping this section
originally described, and widened again for a general vector/matrix pass.
It now provides:

* `oryx::math` (`Functions.h`) — generic templated wrappers around
  `<cmath>` (`sqrt`, `abs`, `sin`, `cos`, `pow`, `tan`, `atan2`, `floor`,
  `ceil`, `round`, `exp`, `log`, `log2`, `min`, `max`, `clamp`, `lerp`),
  named constants (`PI<T>`, `TWO_PI<T>`, `HALF_PI<T>`, `EPSILON<T>`), and
  `sign`, `saturate`, `smoothstep`, `radians`/`degrees`, `approx_equal` —
  so templated math code has one uniform call surface instead of relying
  on ADL over the raw `std::` overloads. Everything that's pure arithmetic
  (`min`/`max`/`clamp`/`lerp`/`sign`/`saturate`/`smoothstep`/`radians`/
  `degrees`) is `constexpr`; anything that calls into `<cmath>` isn't.
* `Vector<N, T>` (`Vector.h`) — a generic vector with the dimension `N` as
  a non-type template parameter, backed by a plain C array (not
  `std::array`). Provides `operator[]`, conditional `x()`/`y()`/`z()`/`w()`
  accessors (via C++20 `requires` clauses, only available when `N` is
  large enough), compound-assignment operators, unary negation, `!=`, and
  componentwise `*`/`/` against another `Vector`. Both member (`length()`,
  `normalized()`, `sum()`, `mean()`, `distance()`, `distance_squared()`)
  and free-function (`dot`, `length`, `normalize`, `sum`, `mean`,
  `distance`, `distance_squared`, `lerp`, `clamp`, `min`, `max`, `abs`,
  `approx_equal`, `to_string`) forms exist for the operations above where
  a primary receiver makes sense — a deliberate numpy/GLM-style dual API,
  see the naming-convention exception in [Design: Naming Conventions](design/cpp-api.md#naming-conventions). `dot`/`cross` and
  the operators stay free-function-only, matching the original design;
  `distance`/`distance_squared` are a deliberate exception (see the
  comment in `Vector.h`).
* `Vector2.h` / `Vector3.h` / `Vector4.h` — dedicated headers for the
  `Vec2f`/`Vec2d`/`Vec2i`, `Vec3f`/`Vec3d`/`Vec3i`, and `Vec4f`/`Vec4d`/
  `Vec4i` aliases. `Vector2.h`/`Vector3.h` also carry their respective
  `cross()` free functions (2D cross is a scalar perp-dot-product, 3D
  cross returns a `Vector<3, T>`) and, on `Vector2.h`, `manhattan_distance`/
  `chebyshev_distance` for grid/board distance queries. `Vector4.h` has no
  `cross()` — there's no natural 4D analog in scope.
* `Matrix<R, C, T>` (`Matrix.h`) — construction, element access
  (`at(row, col)`), `operator*` (matrix×matrix and matrix×vector),
  `operator+`/`operator-`/scalar `operator*`/`operator==`, `transpose()`,
  and a square-only `identity()`. `determinant()`/`inverse()` are
  supported only as a bounded 2×2/3×3 special case, exposed as fully
  specialized free-function overloads (and mirrored as members via a
  `requires` constraint on the class's own `R`/`C`) so calling them on any
  other size is a compile error — not a general N×N algorithm. A singular
  matrix passed to `inverse()` trips an `OX_CORE_ASSERT` in Debug builds
  and falls through to IEEE `Inf`/`NaN` in Release, where the assert
  compiles out. `Matrix2f`/`Matrix2d`/`Matrix4f`/`Matrix4d` aliases
  (`Mat2f`/`Mat2d`/`Mat4f`/`Mat4d`) live directly in `Matrix.h`; `Matrix3.h`
  adds `Mat3f`/`Mat3d` plus 2D affine transform helpers —
  `translation`/`rotation`/`scale` (each returning a `Mat3`) and
  `transform_point` (affine-only, via homogeneous coordinates, no
  perspective divide, no separate `Transform` class). `Matrix.h` depends
  on `Vector.h` (for matrix×vector multiply) and `Core/Assert.h` (for the
  `inverse()` guard) — the first edges from `Math` to another module, both
  harmless since `Core` is the foundational layer already pulled in first
  everywhere.
* `Colour.h` — a minimal, plain data-only `struct Colour { float r, g, b,
  a }` (opaque black by default) with free `operator==`/`operator!=`/
  `approx_equal`/`lerp`. Deliberately **not** given the Vector/Matrix dual
  member+free-function API — that documented exception ([Design: Naming Conventions](design/cpp-api.md#naming-conventions))
  is scoped to exactly those two types. No byte-based variant and no
  named-colour palette (`White`/`Black`/...) — deferred until a real
  renderer/texture-format consumer exists ([Design Review Principle](design/principles.md#design-review-principle)).
* `Math.h` — an umbrella header aggregating the above, included directly
  from the precompiled header (`oxpch.h`), and `Math.cpp` — explicit
  template instantiation of the common `Vector<2|3|4, float|double|int>`
  aliases and the named square `Matrix<2|2|3|3|4|4, float|double>` shapes
  (no `int` matrices — no grid-data use case, only transform use cases),
  so they're compiled once into the `Oryx` static lib rather than
  re-instantiated per translation unit. Explicit instantiation only
  covers these named/aliased shapes — arbitrary `R×C` matrix usage still
  instantiates per-TU as before.

`Math` remains scoped to what board/grid games, and later graphics, need.
Determinant/inverse are supported only as the bounded 2×2/3×3 special case
described above — general N×N determinant/inverse, 4×4 inverse,
quaternions, a `Matrix4`-based 3D transform pipeline, and other
higher-dimensional types remain out of scope until a real use case
demonstrates the requirement ([Design Review Principle](design/principles.md#design-review-principle)).

---

## 3.5 Outcome

Once `IState::is_terminal()` is true, `IState::outcome()` returns an
`Outcome`: `{ bool is_terminal; Rewards<double> rewards; }`. `Rewards<T>` is
a templated class holding one reward per player, sized at runtime from the
game's player count (not templated on player count, since `IGame`/`IState`
are chosen at runtime through a virtual interface). This represents
2-player zero-sum outcomes today (Tic-Tac-Toe: +1/-1/0) without requiring a
breaking change when N-player or non-zero-sum games arrive later.

## 3.6 Application & Layers

`oryx::Application` (`Oryx/Core`) is the host/bootstrap layer — the
Hazel-style `Application`/`EntryPoint.h`/`create_application()` factory
pattern, unrelated to the Game/Strategy/Engine model above. `Application`
owns a `LayerStack` and drives it from `run()`:

```text
Application::run()
    │
    ▼
 while running:
    for each Layer in the stack (insertion order):
        Layer::update()
```

A `Layer` (`Oryx/Core/Layer.h`) is a base class, not a pure interface, so it
does not take the `I`-prefix (same exception as `Application` — see
[Design: Naming Conventions](design/cpp-api.md#naming-conventions)): it mixes concrete state (a `name()`) with virtuals that
default to no-ops (`attach()`, `detach()`, `update()`, `event()`).
`LayerStack` is responsible for constructing layers — `Application::push_layer<T>(args...)` /
`push_overlay<T>(args...)` forward to `LayerStack`, which builds `T` via
`create_unique<T>`, inserts it (layers before the overlay section, overlays
always after), calls `attach()`, and returns `T&`. On destruction the stack
calls `detach()` on every layer in reverse order.

Layers communicate through `Event` (`Oryx/Events/Event.h`) — a Hazel-style
base with a `handled` flag, an `EventType`/`EventCategory` pair for
identifying and filtering events (`event_type()`, `category_flags()`,
`is_in_category()`), and `EventDispatcher` for dispatching to a
type-specific handler (`dispatcher.dispatch<T>(handler)`, matching on
`T::static_type()`). The `OX_EVENT_CLASS_TYPE`/`OX_EVENT_CLASS_CATEGORY`
macros wire a concrete `Event` subclass's `event_type()`/`name()`/
`category_flags()` up to its `EventType`/`EventCategory` values. New
`EventType`/`Event` pairs get added together, per event, once a real layer
needs one — `AppTick`/`AppTickEvent` (`Oryx/Events/ApplicationEvent.h`) is
the first, and still the only one (no windowing system exists to justify a
`WindowResizeEvent` yet). `Application::post_event(Event&)` propagates an
event top-down (most-recently-pushed layer first, via
`LayerStack::rbegin()`), stopping as soon as a layer sets `handled = true`:

```text
Application::post_event(event)
    │
    ▼
 for each Layer in the stack (reverse/top-down order):
    Layer::event(event)
    stop if event.handled
```

`OasisApp` currently pushes a single concrete layer, `OasisLayer`
(`Oasis/src/OasisLayer.h`), which holds the tick-count demo loop previously
inlined in `OasisApp::update()`. Each tick it posts an `AppTickEvent`
through `Application::post_event()` rather than logging directly, and its
own `event()` override picks that event back up via an `EventDispatcher`
(`dispatcher.dispatch<AppTickEvent>(OX_BIND_EVENT_FN(OasisLayer::handle_app_tick))`)
to do the actual logging. With only one layer in the stack today this is a
round trip to itself, but it exercises the full `Layer`/`Event`/
`EventDispatcher`/`Application::post_event` path end-to-end, not just the
mechanism in isolation — the same path any future second layer would use
to listen in on `OasisLayer`'s ticks.

This is a container decision, not an orchestration-model one: it says
nothing about how game simulation is driven (see `# 5. Execution Model`
below, resolved for Phase 5). `SimulationLayer` (Phase 5,
`Oryx/src/Oryx/Simulation/`) is the first concrete `Layer` defined in Oryx
core rather than in an app — a departure from `OasisLayer` being the only
concrete `Layer` so far. This is justified because driving a batch of
simulations is Engine responsibility, not specific to the `Oasis` demo app:
any application linking `Oryx` can push `SimulationLayer` onto its own
`LayerStack`. Other plausible future layers — a `PythonScriptingLayer`, a
GUI/CLI front-end layer, profiling and benchmarking layers — remain
undecided and unbuilt, and would need the same justification (a genuine
cross-app need) before following `SimulationLayer`'s core-not-app placement
(see [Roadmap](roadmap.md) Phases 6/7/9/11).

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

For Phase 2/3, no dedicated abstraction was introduced — the loop above was
validated directly via a doctest test exercising a minimal game and a small
demonstration loop in `Oasis`.

Phase 5 resolves this: a new `Oryx/src/Oryx/Simulation/` core module
provides `Match` (one game + two strategies → `Outcome`, replacing the ad
hoc loop above with a reusable class) and a batch runner (N repeated
matches → aggregated win/loss/draw counts and aggregate `Rewards<T>`).
`SimulationLayer` (§3.6), a concrete `Layer` defined in the same module,
drives a batch through `Application`'s tick loop; `Oasis` pushes it onto
its `LayerStack` like any other layer. This phase is explicitly
single-threaded — see [Design: Parallelism](design/quality.md#parallelism)/[Decision Log](design/decision-log.md) — batching proves the `Match`/
runner API shape, not throughput.

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
Phase 10 Graphics actually needs to swap in a graphical renderer
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
`register()` call per environment in a shared `__init__.py`). The mechanism
is a self-registering factory: a game or strategy registers a name and a
factory function via a static object in its own `.cpp` file, so adding a
new one never requires editing shared engine code.

This mirrors `build_system/registry.py`'s decorator-based command
auto-discovery already used in this project's Python tooling — a single
`Registry<T>::create("name")`-style lookup gives the same ergonomics as
Gymnasium's `gym.make("name")`, without Gymnasium's `__init__.py`
central-registration file.

`Registry<T>` is built in Phase 4, generic and used for both `IGame` and
`IStrategy` immediately — not deferred until a second game exists.
Registration is macro-based: `OX_REGISTER_GAME(TicTacToeGame, "tictactoe")`
/ `OX_REGISTER_STRATEGY(RandomStrategy, "random")` expand to the static
registrar boilerplate (a static object whose constructor calls
`Registry<T>::register_factory(name, factory_fn)`), so an author writes one
macro line per type instead of hand-writing a registrar struct. The macro
expansion happens at that translation unit's compile time; the resulting
static object runs the actual registration at static-initialization time,
before `main()`.

Strategies that are game-specific (e.g. Phase 4's TicTacToe heuristic, kept
in `Oasis` — see [Roadmap](roadmap.md) Phase 4) register under a namespaced name
(`"tictactoe/heuristic"`) rather than a global one (`"random"`).
`Registry<T>` does not enforce game/strategy compatibility — misusing a
game-specific strategy against the wrong game is the caller's
responsibility, matching the same don't-build-enforcement-before-it's-needed
reasoning as `IBoard` (§8).

### Oasis

`Oasis` is a companion executable project (`Oasis/`) that links against `Oryx`
the same way `tests/` does. It is where games, demos, and experiments that
consume the engine should live, so that this kind of content never needs to be
compiled into `Oryx` itself. It is currently a scaffold — the smallest possible
program proving the link works — and is expected to grow into the home for the
first reference game described in [Roadmap](roadmap.md) Phase 3.

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

The Phase 2 brainstorm (see [Design: Decision Log](design/decision-log.md)) resolved the
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
* Plugin architecture — resolved: self-registering-factory principle (§10),
  now built as a generic, macro-based `Registry<T>` (Phase 4), covering
  both `IGame` and `IStrategy`

The Phase 4/5 brainstorm (see [Design: Decision Log](design/decision-log.md)) resolved
further, for this phase's scope:

* `Registry<T>` implementation and timing — resolved: built in Phase 4,
  generic, macro-based self-registration (`OX_REGISTER_GAME`/
  `OX_REGISTER_STRATEGY`), covering both `IGame` and `IStrategy` (§10)
* Simulation orchestration — resolved: `Match` + batch runner in
  `Oryx/Simulation`, `SimulationLayer` drives batches via `Application`'s
  tick loop (§5, §3.6)
* Game-specific strategy registration — resolved: namespaced registry
  names (e.g. `"tictactoe/heuristic"`); no compatibility enforcement (§10)
* `IState` growth for search (clone/copy) — resolved: not needed;
  `apply()`/`undo()` suffices for Minimax lookahead (§3.2)
* Strategy randomness wiring — resolved: no `IStrategy` change;
  `RandomStrategy` owns its own `oryx::Random` (§3.3)
* Capability/`Context` mechanism — resolved ahead of schedule, as
  forward-looking infrastructure: `IStrategy::decide(const Context&)`,
  capabilities explicitly `provide()`d rather than `dynamic_cast`-discovered
  (§3.3, [Design: Decision Log](design/decision-log.md)). No concrete capability ships yet; construction
  lives in `OasisLayer` until a real `Engine`/`Match` exists (still open,
  below)

The following should **not** be considered settled yet:

* Action decode/interpretation capability (e.g. an `IActionFeatures`-style
  capability exposing structured access to what an `ActionId` means, for a
  strategy that needs more than the opaque integer) — planned for Phase 5,
  not yet built; `ActionId` itself stays the opaque wire type (§3.2,
  [Design: Decision Log](design/decision-log.md) "Action representation" — not reopened)
* Player/agent model beyond strict alternation (chance players,
  simultaneous-move players)
* Simultaneous actions
* Chance/nature actions
* Imperfect information
* Game history
* Evaluation API
* Experiment representation
* Observability protocol
* Dashboard transport
* Python ownership/lifetime semantics
* Serialization
* Graphics abstraction
* Multi-threaded simulation model — explicitly deferred rather than merely
  unaddressed: Phase 5's batch runner is single-threaded by design
  ([Design: Parallelism](design/quality.md#parallelism)/[Decision Log](design/decision-log.md)), not pending a decision

These should be addressed systematically rather than solved piecemeal during implementation.

---

# 15. Architectural Principle

When in doubt:

> **Keep the core smaller.**

An abstraction should earn its place by solving a real recurring problem.

Oryx should favour composition, explicit interfaces, dependency inversion, testability, and extension points while avoiding framework-like complexity that is not justified by actual use cases.
