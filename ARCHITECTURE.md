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

The Game abstraction represents a particular game or decision environment.

A game is responsible for:

* Representing state
* Defining legal actions
* Applying actions
* Determining terminal states
* Producing outcomes
* Defining game-specific rules

A game should not:

* Select actions on behalf of an agent
* Know which strategy is being used
* Depend on the visualisation system
* Depend on experiment infrastructure

A game should be usable independently of graphics and the Strategy Dashboard.

---

## 3.2 Strategy

A Strategy represents decision-making.

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

Providing reproducible random-number generation and seed management.

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

The following should **not** be considered settled yet:

* Exact `Game` interface
* Exact `Strategy` interface
* State ownership and representation
* Action representation
* Player/agent model
* Turn management
* Simultaneous actions
* Chance/nature actions
* Imperfect information
* Game history
* Randomness abstraction
* Simulation orchestration
* Evaluation API
* Experiment representation
* Result model
* Observability protocol
* Dashboard transport
* Plugin architecture
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
