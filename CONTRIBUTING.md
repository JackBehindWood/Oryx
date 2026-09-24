# Contributing to Oryx

Thank you for your interest in contributing to Oryx! This document outlines guidelines and processes for contributing to the project.

## Code of Conduct

We are committed to providing a welcoming and inclusive environment for all contributors. Please be respectful and constructive in all interactions.

## How to Contribute

### Reporting Bugs

If you find a bug:

1. Check the [issue tracker](https://github.com/JackBehindWood/oryx/issues) to see if the bug has already been reported.
2. If not, open a new issue with:
   - Clear title describing the bug
   - Detailed description of the problem
   - Steps to reproduce
   - Expected vs. actual behavior
   - Environment (OS, compiler version, Python version, etc.)
   - Minimal reproducible example if possible

### Suggesting Features

To suggest a feature or improvement:

1. Check existing [issues](https://github.com/JackBehindWood/oryx/issues) and [discussions](https://github.com/JackBehindWood/oryx/discussions).
2. Open a new discussion or issue with:
   - Clear description of the feature
   - Motivation and use case
   - Example of how it would be used
   - Any relevant context or references

### Submitting Changes

#### Setup

1. Fork the repository on GitHub.
2. Clone your fork locally:
   ```bash
   git clone https://github.com/your-username/oryx.git
   cd oryx
   git submodule update --init --recursive
   ```
   (`tests/vendor/doctest`, the test framework, is a git submodule — without
   this step `uv run forge compile` will fail with a clear message
   telling you to run it.)

#### Build & test

The build/tooling CLI is `pyforge` (`forge` on the command line), documented in
[`build_system/pyforge/README.md`](build_system/pyforge/README.md) and
[`docs/tooling.md`](docs/tooling.md). Two ways to get it:

**uv (recommended)** — a single command installs pyforge, its `menu`/`plugins` extras and
Oryx's own dev dependencies (research, docs, test) into one workspace:
```bash
uv sync
uv run forge all          # configure, compile, test
uv run forge run          # run the Oasis sandbox
```

**Plain pip** — no uv required; installs pyforge as a standalone package:
```bash
python -m venv .venv && source .venv/bin/activate    # .venv\Scripts\activate on Windows
pip install -e "build_system/pyforge[menu,plugins]"
pip install --group research pytest
forge all
```

Both call the same `forge.toml`-driven CLI; `uv sync`'s workspace is only a convenience for
working on Oryx and pyforge together. If you're changing pyforge itself, its own test suite
(`cd build_system/pyforge && pytest`) is faster to iterate against than a full Oryx build.