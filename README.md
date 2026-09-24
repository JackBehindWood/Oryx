# Oryx — Game Strategy Engine

Oryx is an open-source, extensible **Game Strategy Engine (GSE)** for implementing games, strategies, simulation, experimentation, optimisation, algorithm analysis, and visualisation.

The goal is to provide a small, powerful core that can support both serious engineering/research and learning.

Oryx is not intended to become a general-purpose game engine. Its focus is on the intersection of:

* Games and game theory
* Decision-making under uncertainty
* Operations Research
* Probability and stochastic processes
* Search and optimisation
* Monte Carlo methods
* Reinforcement learning
* Algorithm experimentation and analysis
* Educational visualisation

The project is being developed incrementally. The architecture and API described in this document are intentionally subject to refinement as the project evolves.

---

## Core Philosophy

The fundamental architectural principle is:

> **Game != Strategy != Engine**

These are separate responsibilities.

### Game

A **Game** defines the problem being played or simulated.

It owns concepts such as:

* Rules
* State
* Legal actions
* State transitions
* Terminal conditions
* Outcomes
* Game-specific information

A game should not know how an agent decides what to do.

### Strategy

A **Strategy** decides what action to take.

Examples may include:

* Random strategies
* Heuristics
* Minimax
* Monte Carlo Tree Search
* Reinforcement-learning policies
* Optimisation-based strategies
* Game-theoretic strategies

A strategy should operate through game abstractions rather than depending on a particular game implementation.

### Engine

The **Engine** provides the infrastructure for executing and studying games and strategies.

Potential responsibilities include:

* Simulation
* Evaluation
* Experiment execution
* Randomness management
* Statistics
* Benchmarking
* Search infrastructure
* Strategy instrumentation
* Reproducibility
* Shared utilities

The engine should not contain game-specific rules or strategy-specific decision logic.

---

## Project Vision

Oryx should make it straightforward to move through the following workflow:

```text
Implement a Game
      ↓
Implement a Strategy
      ↓
Run Simulations
      ↓
Collect Results
      ↓
Compare Algorithms
      ↓
Inspect Decisions
      ↓
Visualise Behaviour
      ↓
Run Experiments
      ↓
Analyse Results
      ↓
Explain Algorithms
```

A game or strategy should normally be addable without modifying the core engine.

---

## Technology

### C++

C++ is the primary language for the engine.

It is intended for:

* Core abstractions
* Game implementations where performance matters
* Simulation
* Search
* Monte Carlo methods
* Optimisation
* High-performance algorithms
* Shared engine infrastructure

The project targets modern, readable C++ while avoiding complexity for its own sake.

### Python

Python is the experimentation, research, analysis, and tooling layer.

It is intended for:

* Experimentation
* Notebooks
* Statistics
* Visualisation
* Benchmarking
* Configuration
* Strategy prototyping
* Reinforcement learning workflows
* Research tooling
* Development tooling

The Python API should be idiomatic and high-level rather than simply exposing the C++ implementation directly.

### Python Bindings

[pybind11](https://github.com/pybind/pybind11) is the intended binding technology unless a strong technical reason emerges to use an alternative.

### Build System

Oryx uses **Premake5** for C++ project generation and build configuration.

The project also includes a small Python development CLI.  See [`build_system`](build_system/README.md) for contribution guidelines.

The CLI is intended to simplify common workflows without replacing Premake5.
Run `uv run forge` with no arguments for an interactive arrow-key menu, or use
direct subcommands for scripts and CI:

```text
uv run forge                          # interactive menu
uv run forge config init --ide vscode # forge.local.toml + .vscode/{tasks,launch,...}.json
uv run forge build all                # configure, compile, test
uv run forge build run                # run the Oasis sandbox executable
uv run forge docs serve               # live-preview the documentation site
build experiment
build explain
```

The exact command structure will evolve as the project develops.

`Oasis` is the companion sandbox executable that links against `Oryx` — the
home for games, demos, and experiments that consume the engine without being
compiled into it. See [the architecture docs](docs/architecture.md#10-extension-model) for
how it fits into the extension model.

---

## Headless First

The engine must support headless execution.

Graphics are optional infrastructure rather than a requirement of the core engine.

This allows Oryx to support:

* Large-scale simulations
* Server execution
* Automated experiments
* CI
* Benchmarking
* Research workflows
* Reinforcement-learning environments

without requiring a graphical environment.

---

## Strategy Observability

One of Oryx's longer-term goals is to make strategy behaviour observable.

Strategies may optionally expose information such as:

* Selected actions
* Action probabilities
* Expected values
* Value estimates
* Search depth
* Simulations performed
* Nodes explored
* Search trees
* Probability distributions
* Constraints
* Optimisation variables
* Regret
* Convergence
* Performance metrics
* Decision traces
* Algorithm-specific diagnostics

Observability is intentionally optional.

A strategy should remain usable without the Strategy Dashboard, and different strategies should be able to expose different information.

---

## Repository Status

Oryx is currently in an early architectural and design phase.

The current documentation establishes the project's direction and initial architectural boundaries, but the detailed architecture, APIs, and implementation strategy are expected to evolve following dedicated design and architecture sessions.

In particular, the following are intentionally not considered final:

* Core C++ interfaces
* Game state representation
* Action representation
* Strategy interfaces
* Simulation architecture
* Experiment model
* Observability API
* Python API
* Graphics architecture
* Plugin/extension mechanisms

The project favours **incremental design over speculative implementation**.

---

## Documentation

| Document               | Purpose                                                    |
| ---------------------- | ---------------------------------------------------------- |
| `README.md`            | Project overview and getting started                       |
| `docs/architecture.md` | Architectural structure and component boundaries           |
| `docs/design/`         | Technical principles, decisions, and open design questions |
| `docs/roadmap.md`      | Development direction and planned milestones               |
| `CONTRIBUTING.md`      | Contribution guidelines                                    |

The `docs/` folder is a MkDocs site: run `uv run forge docs serve` to preview it locally.

----------------- | ---------------------------------------------------------- |
| `README.md`       | Project overview and getting started                       |
| `docs/architecture.md` | Architectural structure and component boundaries      |
| `docs/design/`    | Technical principles, decisions, and open design questions |
| `docs/roadmap.md` | Development direction and planned milestones               |
| `CONTRIBUTING.md` | Contribution guidelines                                    |

---

## Contributing

Contributions are welcome.

Before making substantial architectural changes, please review the architecture and design documentation and consider opening a discussion first.

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for contribution guidelines.

---

## License

License information will be added as the project is formalised.
