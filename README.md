# Oryx

An open-source engine for games, strategies, simulation, and decision-making.

**Status:** Foundation phase — establishing core architecture and build infrastructure.

## What is Oryx?

Oryx is a small, powerful, extensible platform for:

- **Games** — rule definitions, state, legal actions, outcomes
- **Strategies** — decision-making algorithms, action selection, agent behavior
- **Simulation** — executing strategies against games, analyzing outcomes
- **Experimentation** — framework for research and algorithm exploration
- **Optimization** — decision-making under uncertainty, uncertainty quantification
- **Education** — learning game theory, algorithms, and strategy optimization

## Vision

A long-term vision is to build a small, powerful engine surrounded by an ecosystem of algorithms, simulations, visualizations, and educational tools—useful to software engineers, researchers, students, game developers, AI/ML practitioners, and educators.

## Core Principle: Game ≠ Strategy ≠ Engine

Oryx enforces clean separation:

- **Game**: Defines rules, state, legal actions, transitions, outcomes
- **Strategy**: Defines decision-making and action selection
- **Engine**: Provides execution, simulation, evaluation, infrastructure

These responsibilities must remain loosely coupled. New games or strategies should be addable without modifying the core engine.

## Current Status

Oryx is in its **foundation phase**:

- ✅ Project structure established
- ✅ Build system (Premake5 + Python orchestrator) working
- ✅ Minimal C++ scaffold to verify compilation
- ⏳ Core abstractions (not yet implemented)
- ⏳ First game implementation
- ⏳ First strategy implementation
- ⏳ Python bindings (pybind11)

**Do not assume any engine features exist yet.** This is a clean foundation for deliberate, minimal development.

## Building Oryx

### Prerequisites

- Python 3.9+
- C++17 compiler (GCC, Clang, or MSVC)
- `make` (Linux/macOS) or equivalent build tools

### Build

```bash
# Configure and build
python build.py build

# Run tests (if any exist)
python build.py test

# Clean build artifacts
python build.py clean

# All-in-one
python build.py all
```

The build system automatically downloads and manages Premake5 in the `premake/` directory on first run.

## Repository Structure

```
oryx/
├── include/           # C++ public headers
│   └── oryx/
├── src/               # C++ implementation (placeholder)
├── tests/             # C++ tests
├── research/          # Python research, analysis, tooling
├── build.py           # Python build orchestrator
├── premake5.lua       # Premake5 build configuration
├── LICENSE            # Apache 2.0
├── CONTRIBUTING.md    # Contribution guidelines
└── README.md          # This file
```

## Development

- **Build system**: Premake5 (via `build.py`)
- **Primary language**: C++17
- **Research/tooling**: Python
- **Python bindings**: pybind11 (future)

For contribution guidelines and architecture details, see [CONTRIBUTING.md](CONTRIBUTING.md).

## License

Oryx is licensed under the Apache License 2.0. See [LICENSE](LICENSE) for details.

## Questions?

- Open an issue for bugs or feature requests
- Open a discussion for questions and ideas
- See [CONTRIBUTING.md](CONTRIBUTING.md) for development guidelines
