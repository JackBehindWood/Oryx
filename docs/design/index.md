# Design

> **Status: Preliminary — design principles and working assumptions**

These pages record the technical design philosophy behind Oryx.

They are deliberately different from the [Architecture](../architecture.md).

* The [Architecture](../architecture.md) describes **what components exist and how they relate**.
* The design pages describe **why we are making particular technical choices** and which questions remain open.

The decisions here are working assumptions until reviewed during the project's dedicated architecture and design phase.

| Page | Covers |
| ---- | ------ |
| [Principles](principles.md) | Design goals, Game != Strategy != Engine, composition, small interfaces, the design review principle |
| [C++ API](cpp-api.md) | Naming conventions, math module, error handling |
| [Python API](python-api.md) | Python API design and the binding layer |
| [Determinism](determinism.md) | Randomness, determinism and reproducibility |
| [Quality](quality.md) | Testing, performance and the allocation audit, parallelism |
| [Observability](observability.md) | Optional strategy observability |
| [Platform](platform.md) | Serialization, graphics, build system, documentation |
| [Decision Log](decision-log.md) | What is settled, what is open, and why |
