# Oryx Design

> **Status: Preliminary — design principles and working assumptions**

This document records the technical design philosophy behind Oryx.

It is deliberately different from `ARCHITECTURE.md`.

* `ARCHITECTURE.md` describes **what components exist and how they relate**.
* `DESIGN.md` describes **why we are making particular technical choices** and which questions remain open.

The decisions below are working assumptions until reviewed during the project's dedicated architecture and design phase.

---

# 1. Design Goals

Oryx should optimise for:

1. Simplicity
2. Extensibility
3. Correctness
4. Testability
5. Reproducibility
6. Performance where it matters
7. Good developer experience
8. Good research experience
9. Educational value
10. Long-term API stability

Performance is important, but unnecessary complexity should not be introduced before a real performance requirement exists.

---

# 2. Game != Strategy != Engine

This is the most important design constraint.

A game defines:

```text
What can happen?
```

A strategy defines:

```text
What should I do?
```

The engine defines:

```text
How do we execute, simulate, evaluate, and study it?
```

This separation should be maintained even when doing so requires slightly more explicit interfaces.

---

# 3. Prefer Composition

Oryx should generally prefer composition over deep inheritance hierarchies.

For example, a strategy may be composed from:

```text
Search
+
Evaluation
+
Randomness
+
Policy
+
Observability
```

rather than requiring every algorithm to inherit from an increasingly large base class.

Inheritance should be used when there is a genuine stable "is-a" relationship and polymorphism provides clear value.

---

# 4. Small Interfaces

Interfaces should expose only the capabilities actually required.

Avoid a universal interface such as:

```cpp
class Everything
{
    ...
};
```

A game that only requires legal-action generation should not be forced to implement unrelated concepts.

This is particularly important because Oryx may eventually support very different classes of games:

* Deterministic games
* Stochastic games
* Perfect-information games
* Imperfect-information games
* Simultaneous-action games
* Single-player environments
* Multi-agent environments

The abstractions should grow from demonstrated requirements.

`IGame`, `IState`, and `IStrategy` (see `ARCHITECTURE.md` §3) are examples
of this: each pure-virtual interface exposes only what callers actually
need, using an `I`-prefix reserved specifically for fully pure-virtual
interfaces (see Naming Conventions in §5).

---

# 5. C++ API Design

C++ is the core implementation language.

The C++ API should prioritise:

* Clear ownership
* Explicit lifetimes
* Value semantics where practical
* Minimal hidden allocation
* Const-correctness
* Testability
* Predictable performance
* Readable modern C++

Templates should be used where they provide meaningful benefits, not merely because they are available.

Likewise, advanced metaprogramming should not become a prerequisite for understanding the engine.

### Naming Conventions

* Functions and methods: `snake_case` (e.g. `legal_actions()`)
* Classes: `PascalCase` (e.g. `class Rewards`)
* Pure-virtual interfaces (100% pure virtual, no data, no concrete methods)
  additionally get an `I`-prefix (e.g. `IGame`, `IState`, `IStrategy`).
  Base classes that mix concrete behaviour with virtual methods do not —
  e.g. `Application` keeps its name (it has real state — `m_running`, the
  `LayerStack` — and concrete methods like `run()`/`close()`/`push_layer()`);
  `Layer` (`Oryx/Core/Layer.h`) is the same case, with a concrete `name()`
  alongside `attach()`/`detach()`/`update()`/`event()` virtuals that default
  to no-ops rather than being pure.
* Structs are data-only: plain fields, no member functions. Any behaviour
  needed on struct-held data is a free function instead (e.g. `Outcome`
  has `is_terminal`/`rewards` fields only; `Colour` has `r`/`g`/`b`/`a`
  fields only, with `operator==`, `approx_equal` and `lerp` as free
  functions).
* Exception — math types (`Vector<N, T>`, `Matrix<R, C, T>`): these are
  `class`, not data-only structs, and deliberately expose *both* member
  functions and equivalent free functions for the operations where a
  primary receiver makes sense. For `Vector`: `length()`, `normalized()`,
  `sum()`, `mean()`, `distance()`, `distance_squared()` as members, with
  `dot`, `length`, `normalize`, `sum`, `mean`, `cross`, `distance`,
  `distance_squared`, `lerp`, `clamp`, `min`, `max`, `abs`, `approx_equal`,
  `to_string` as free functions (operators and `dot`/`cross` stay
  free-function-only; `distance`/`distance_squared` are a deliberate
  exception to that, noted inline in `Vector.h`). For `Matrix`:
  `transpose()`, `determinant()`, `inverse()` (the latter two bounded to
  2×2/3×3) as members, with the same plus the arithmetic operators as free
  functions. This mirrors the dual method/module-function API convention
  used by numpy and similar math libraries (GLM, Eigen), and is a
  documented, intentional carve-out scoped to `Vector`/`Matrix` — it does
  not loosen the data-only-struct rule elsewhere (`Colour` deliberately
  stays a plain data-only struct, not part of this exception).

This was not written down before the Phase 2 brainstorm; existing code
predates it and is not being retrofitted (e.g. `Application::Get()` is a
static accessor, `create_application()` a free function — both fine as
historical exceptions, not examples to copy for new pure interfaces).

### Math Module

A header-only `oryx::Math` module (`Oryx/src/Oryx/Math/`), widened during
Phase 2 planning from the originally-scoped "`Vec2` struct", and widened
again for a general vector/matrix/colour pass (see `ARCHITECTURE.md` §3.4
for the full breakdown). It provides `oryx::math` (templated `<cmath>`
wrappers, plus named constants `PI`/`TWO_PI`/`HALF_PI`/`EPSILON` and
`sign`/`saturate`/`smoothstep`/`radians`/`degrees`/`approx_equal`), a
generic `Vector<N, T>` with dimension `N` as a non-type template parameter
(backed by a plain C array, not `std::array`), dedicated
`Vector2.h`/`Vector3.h`/`Vector4.h` headers for the `Vec2`/`Vec3`/`Vec4`
aliases (2D/3D also get `cross()`; 2D also gets `manhattan_distance`/
`chebyshev_distance`), a `Matrix<R, C, T>` with matrix×matrix and
matrix×vector multiply, `+`/`-`/scalar `*`/`==`, `transpose()`, and a
bounded 2×2/3×3-only `determinant()`/`inverse()` (never a general N×N
algorithm), `Matrix3.h`'s `translation`/`rotation`/`scale`/
`transform_point` 2D affine-transform helpers (via homogeneous
coordinates, no `Transform` class), and a minimal `Colour` struct:

```cpp
template<size_t N, typename T>
class Vector
{
public:
    T& operator[](size_t i);
    T length() const;
    Vector<N, T> normalized() const;
    // ...
private:
    T m_data[N]{};
};

using Vec2f = Vector<2, float>;
using Vec3f = Vector<3, float>;

struct Colour
{
    float r = 0.0f, g = 0.0f, b = 0.0f, a = 1.0f;
};
```

`Vector<N,T>`/`Matrix<R,C,T>` are the documented exception to the
struct-is-data-only rule above; `Colour` deliberately is **not** — it stays
a plain data-only struct, with `operator==`/`approx_equal`/`lerp` as free
functions, since its behaviour is small enough that extending the
Vector/Matrix exception to a third type isn't warranted. The module is
still scoped to what board/grid games and, later, graphics actually need —
not a general-purpose maths library. The 2×2/3×3 `determinant`/`inverse`
are a deliberate, bounded addition (special-cased free-function overloads,
not a general algorithm) to unblock real 2D transform math; general N×N
determinant/inverse, 4×4 inverse, quaternions, a `Matrix4`-based 3D
transform pipeline, and a byte-based/named-colour palette remain out of
scope and should grow only from demonstrated requirements (§20).

---

# 6. Python API Design

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

---

# 7. Bindings

pybind11 is the current intended binding technology.

The binding layer should act as an API boundary rather than exposing every internal C++ type.

Internal C++ implementation details should remain internal where possible.

This reduces Python API churn and allows the C++ implementation to evolve independently.

---

# 8. Randomness

Randomness is fundamental to many intended use cases.

The design should eventually distinguish between:

```text
Randomness source
        ↓
Game randomness
Strategy randomness
Experiment randomness
```

Reproducibility requires explicit control over random-number generation.

We should avoid hidden global random state.

The final design should answer:

* Who owns the RNG?
* How are seeds assigned?
* How are parallel simulations seeded?
* Can individual components have independent streams?
* How are random states reproduced?

These questions should be resolved before the simulation system becomes substantial.

As of the Phase 2 brainstorm, a standalone, seedable `oryx::Random` utility
(wrapping `std::mt19937_64`) has been added to `Oryx/Core`. It is not yet
wired into `IGame`/`IState` — Phase 2/3 have no chance nodes — but exists so
Phase 4's Random strategy, and later reproducibility work, have a single
reproducible source rather than each reaching for `<random>` independently.

Phase 4 resolves the strategy-level question: `RandomStrategy` owns its own
`oryx::Random` member, seeded via its own constructor argument. `IStrategy`
gained no seed/RNG parameter — seeding a batch's strategies is instead the
responsibility of whatever constructs them, i.e. the `Match`/batch-runner
level introduced in Phase 5 (`ARCHITECTURE.md` §5), not the strategy
interface itself.

---

# 9. Determinism and Reproducibility

The same experiment configuration should, where possible, be reproducible.

A reproducible experiment should ideally record:

```text
Game
Strategy
Parameters
Seed
Number of runs
Oryx version
Relevant environment information
Results
```

Parallel execution introduces additional challenges.

The system should distinguish between:

* Deterministic game logic
* Deterministic single-threaded execution
* Reproducible seeded simulations
* Bit-for-bit reproducibility

These are not necessarily equivalent.

---

# 10. Error Handling

The project should favour errors that are:

* Explicit
* Actionable
* Easy to diagnose
* Testable

C++ exceptions may be appropriate for exceptional failures, while ordinary game operations should preferably make invalid states difficult to create.

The precise error-handling policy should be established before public APIs become stable.

---

# 11. Testing

Testing should exist at several levels.

### Unit tests

For:

* Game rules
* State transitions
* Action validation
* Strategy components
* Utility algorithms

### Integration tests

For:

* Game + strategy execution
* Simulation
* Engine workflows
* Python bindings

### Regression tests

For:

* Known algorithm behaviour
* Deterministic scenarios
* Previously discovered bugs

### Statistical tests

Where appropriate for stochastic algorithms.

Tests should avoid assuming that a stochastic result must equal one exact outcome unless the seed and execution model guarantee it.

---

# 12. Performance

The project should follow:

> Measure first, optimise second.

Likely performance-sensitive areas include:

* State copying
* Action generation
* Simulation loops
* Search trees
* Monte Carlo rollouts
* Memory allocation
* Python/C++ boundary crossings
* Parallel simulation

Performance abstractions should be introduced based on profiling and benchmark evidence.

### Allocation audit (2026-09)

A first pass audited where `Oryx`/`Oasis` actually allocate, using the new
`oryx::Instrumentation`/`ScopeTimer` utility (`Oryx/Debug/Instrumentation.h`)
and `tests/benchmark/bench_minimax_allocation.cpp`. Classification:

* **Hot** (scales with search-tree nodes, not just matches/turns):
  `IState::legal_actions()` — a fresh heap `std::vector<ActionId>` per call,
  invoked once per node by `MinimaxStrategy`'s unmemoized recursive search
  (`MinimaxStrategy.cpp`); `Rewards<double>` return-by-value in
  `MinimaxStrategy::evaluate()` (also a heap vector, via `Outcome.h`).
* **Warm** (once per turn or per match): `Context`'s
  `std::unordered_map<std::type_index, void*>` and
  `Match::missing_capabilities()` (once per `decide()`, i.e. per turn, not
  per node); `IGame::new_initial_state()` and `ActionHistory`'s two vectors
  (once per `Match`, in `BatchRunner::run()`'s per-match loop).
* **Cold** (once per process): `Registry<T>` entries, `IGame`/`IStrategy`
  construction. `oryx::UniquePtr`/`SharedPtr` (`Base.h`) are thin aliases
  used only for these one-shot constructions — not implicated here.

Benchmark evidence (DummyGame, pile size 20, full exhaustive Minimax search,
Debug build, Apple Silicon): 144,664 `legal_actions()` calls took ~24% of
total `decide()` wall time even with instrumentation overhead included. An
isolated micro-benchmark comparing a heap `std::vector` holding a
`legal_actions()`-shaped (<=3 element) result against an equivalent
fixed-size stack array found ~717 ns/call for the heap vector vs. ~1.8
ns/call for the array — roughly 390x, purely from heap traffic on a
result that's always small.

**Decision: narrow fix identified, not yet implemented.** The evidence
supports a small-buffer-optimized (SBO) vector-like return type for
`IState::legal_actions()` specifically — sized inline for the branching
factors seen so far (<=9, e.g. Tic-Tac-Toe/`DummyGame`) — rather than a
general-purpose pool/arena allocator (no evidence yet that `Match`/
`BatchRunner`-level allocations are hot enough to justify one) or touching
`UniquePtr`/`SharedPtr` (not implicated by this audit). Implementing the SBO
container is deliberately deferred to a separate change, gated on this
finding rather than bundled with the audit itself (§20).

**Outcome (2026-09, implemented).** `SmallVector<T, N>` (`Oryx/Containers/SmallVector.h`)
replaced `std::vector` as `IState::legal_actions()`'s return type
(`ActionList = SmallVector<ActionId, 9>`) and `Rewards<T>`'s internal storage.
Re-running `bench_minimax_allocation.cpp`: `legal_actions()`'s share of
`MinimaxStrategy::decide()` wall time dropped from ~24% to ~6.2% (144,664
calls, same `DummyGame` workload); the isolated micro-benchmark's ~707ns/call
heap-vector cost fell to ~31.6ns/call for `ActionList` (~22x — vs. ~1.8ns/call
for a raw fixed array; some per-push overhead remains, but the heap
allocation is gone).

Beyond the audited fix, at the user's explicit direction and without separate
benchmarking (§18/§20 — recorded here rather than silently decided in code),
the same container was also applied to `Match`/`BatchRunner`/`SimulationLayer`'s
strategy-holding vectors, `BatchResult::wins` (all sized by `num_players()`,
inline capacity 2), and `IActionFeatures::decode()` (inline capacity 2).
`Registry<T>` separately moved from `std::unordered_map` to a new
open-addressing `FlatHashMap<Key, Value, N>` (`Oryx/Containers/FlatHashMap.h`, over
a new `Pair<K,V>` type) — also unaudited, since `Registry<T>` is Cold. Both
containers spill to the heap past their inline capacity, so neither caps
entry/element counts. Left out of scope: `ActionHistory`, the capability
vectors (`missing_capabilities()`/`required_capabilities()`), `Registry::names()`,
game-specific strategy-lookup redesign, and a runtime-configurable-capacity
sibling to `SmallVector` (no current caller needs one).

---

# 13. Parallelism

Large-scale simulation is an important potential workload.

However, concurrency should not be built into every abstraction prematurely.

The likely design direction is to make simulations independently executable where possible:

```text
Experiment
    │
    ├── Simulation 1
    ├── Simulation 2
    ├── Simulation 3
    ├── ...
    └── Simulation N
```

This can provide a natural basis for parallel execution.

Phase 5's batch runner takes the first concrete step here — but stays
single-threaded. Parallel execution is explicitly scoped out of Phase 5
rather than left ambient: batching this phase exists to prove the `Match`/
runner API shape (one game + two strategies → `Outcome`, aggregated over N
runs), not to deliver throughput. The exact threading/executor model
remains open for a later phase.

---

# 14. Observability

Observability should be optional.

A simple strategy should be able to operate with essentially:

```text
Action choose(state)
```

while an instrumented strategy might provide:

```text
Decision
├── action
├── probabilities
├── values
├── search statistics
└── diagnostics
```

Instrumentation should not force every strategy into a common diagnostic model.

Algorithm-specific diagnostics should be possible.

---

# 15. Serialization

Serialization may eventually be useful for:

* Saving game states
* Experiment configuration
* Results
* Replaying decisions
* Debugging
* Reproducibility

However, serialization should not become a mandatory requirement for every core object until a concrete use case justifies it.

---

# 16. Graphics

Graphics should consume game state rather than own game logic.

The renderer should not determine whether a move is legal.

Conceptually:

```text
Game
 │
 └── State ──► Renderer
```

rather than:

```text
Renderer ──► Game rules
```

This allows the same game to run:

* Headlessly
* In a desktop UI
* In an educational visualisation
* In automated tests
* In a server environment

Phase 3's `TicTacToeBoard` (in `Oasis`) is a concrete class, not yet behind
a shared `IBoard` interface — with one game and one renderer, an interface
has no second implementation to justify it. `IBoard` is extracted once
Phase 10 Graphics needs to swap in a graphical renderer polymorphically
(`ARCHITECTURE.md` §8), following the same don't-build-it-before-it's-needed
reasoning as the `Registry<T>` timing decision (§19).

---

# 17. Build System

Premake5 remains the C++ build/project-generation system.

The Python build CLI exists to provide developer ergonomics around it.

The intended separation is:

```text
build_system CLI
   │
   ├── configuration
   ├── Premake invocation
   ├── compilation workflow
   ├── testing
   ├── benchmarks
   └── development commands
          │
          ▼
       Premake5
          │
          ▼
     C++ toolchain
```

The Python CLI should orchestrate rather than duplicate the responsibilities of Premake.

Premake itself defines three sibling projects: `Oryx` (the engine, a static
library), `tests` (the engine's own test suite), and `Oasis` (a console
application that links `Oryx`). `Oasis` exists so that games, demos, and
experiments built on top of the engine have a home that is not the engine
itself — the same reasoning behind keeping the CLI a thin orchestration layer
rather than absorbing Premake's responsibilities.

---

# 18. Documentation as a Design Tool

Documentation is not only for users.

The architecture and design documents should expose unresolved decisions before they become accidental implementation decisions.

When an important question is unresolved, it is preferable to record:

```text
Decision: Open
Options: A / B / C
Reason unresolved: ...
Next step: ...
```

rather than silently choosing an architecture in code.

---

# 19. Current Decision Log

| Decision                        | Current position      | Status                |
| ------------------------------- | --------------------- | --------------------- |
| Primary engine language         | C++                   | Working decision      |
| Research/tooling language       | Python                | Working decision      |
| Python bindings                 | pybind11              | Working decision      |
| C++ build system                | Premake5              | Working decision      |
| C++ test framework              | doctest (git submodule, `tests/vendor/doctest`) | Working decision |
| Developer CLI                   | Python `build`        | Working decision      |
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
| Extension registration          | Self-registering factories, no central list; generic `Registry<T>` built in Phase 4, covering `IGame` and `IStrategy`, registered via `OX_REGISTER_GAME`/`OX_REGISTER_STRATEGY` macros expanding to a static self-registering object per type | Working decision |
| Naming conventions              | `snake_case` functions, `PascalCase` classes, `I`-prefix for pure interfaces, data-only structs | Working decision |
| Board rendering abstraction     | Concrete `TicTacToeBoard` class (Phase 3); `IBoard` deferred to Phase 10 | Working decision (scoped) |
| Application layering            | `Layer`/`LayerStack` owned by `Application` (`ARCHITECTURE.md` §3.6); `LayerStack` constructs layers via `push_layer<T>()`/`push_overlay<T>()`; `run()` drives `update()`, events propagate top-down via `Layer::event()` until handled | Working decision |
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
| Profiling utility                | `oryx::Instrumentation`/`ScopeTimer` (`Oryx/Debug/Instrumentation.h`) — minimal opt-in wall-clock timing pulled forward from Phase 6 to support the allocation audit below; deliberately scoped to a call-site macro (`OX_PROFILE_SCOPE`) and a queryable registry, no `build benchmark` CLI or dashboard yet (ROADMAP.md §8/§17) | Working decision (scoped) |
| `IState::legal_actions()` allocation | Implemented (§12): `SmallVector<ActionId, kActionListInlineCapacity>` (`ActionList`, capacity 9) replaces `std::vector` as the return type; `legal_actions()`'s share of `MinimaxStrategy::decide()` wall time measured dropping from ~24% to ~6.2%. Capacity is a single engine-wide ceiling (the return type of a pure-virtual method) measured from TicTacToe's board (9); a future game with a larger branching factor loses the inline fast path silently (still correct - `SmallVector` spills to heap) - re-measure, don't guess, when one is added. `TicTacToeState::legal_actions()` carries an `OX_CORE_ASSERT` tripwire against the capacity, so a board-shape change that breaks the measured bound fails loudly instead of silently regressing | Resolved (capacity scoped to games measured so far, guarded by assert) |
| SBO container reuse (`Rewards<T>`, strategy vectors, `Registry<T>`) | `Rewards<T>` (separately audited-hot, §12) and `Match`/`BatchRunner`/`SimulationLayer`'s strategy vectors/`BatchResult::wins`/`IActionFeatures::decode()` (sized by `num_players()`, unaudited) moved to `SmallVector`; `Registry<T>` moved from `std::unordered_map` to a new open-addressing `FlatHashMap<Key,Value,N>` + `Pair<K,V>` (unaudited, Cold path) — all user-directed extrapolations beyond §12's evidence | Working decision (unaudited beyond `legal_actions()`) |
| Runtime-configurable-capacity SBO container | Discussed alongside `SmallVector<T,N>`; deferred since no current caller needs a non-compile-time inline capacity. Re-checked (2026-09 follow-up audit) against a possible pybind11 binding (Phase 7 - calls the same `Registry<T>::register_factory()` as any C++ caller, no new code path) and concurrent registration (no threading model exists yet, §13) - neither adds a caller. Still no runtime-configurable container needed | Open (re-affirmed) |
| `Registry<T>`/`FlatHashMap`/`SmallVector` thread-safety | None of the three synchronize; registration is expected single-threaded (self-registering static init). Documented in-code rather than enforced, since no concurrent registration path exists yet (§13 - multi-threaded simulation explicitly deferred) | Working decision (documented, unenforced) |
| `FlatHashMap` erase() | Not implemented; no caller unregisters a game/strategy (process-lifetime singletons, "Extension registration" above). Becomes a real question once Python-side registrants can be garbage-collected mid-process - i.e. at Phase 7, not before | Open (deferred to Phase 7) |
| Game-specific strategy lookup | `Registry<IStrategy>` keeps its flat namespaced-name convention (`"tictactoe/heuristic"`); a nicer per-game lookup API was raised but not specified | Open (deferred, unchanged from prior scoping) |

---

# 20. Design Review Principle

No major abstraction should be introduced simply because it appears useful in theory.

Before adding one, ask:

1. What concrete problem does it solve?
2. Is the problem recurring?
3. Can composition solve it more simply?
4. Does it improve or complicate the public API?
5. Does it preserve Game/Strategy/Engine separation?
6. Does it help both C++ and Python users where appropriate?
7. Can it be tested independently?
8. Does the project actually need it now?

If the answer is unclear, defer the abstraction.

---

# 21. Architecture Phase

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
