# Oryx

Oryx is an open-source Game Strategy Engine: a C++ core for games, strategies, simulation and search, with Python tooling around it.

## Where to start

| Page | Purpose |
| ---- | ------- |
| [Architecture](architecture.md) | What components exist and how they relate |
| [Design](design/index.md) | Why we make particular technical choices, and which questions remain open |
| [Roadmap](roadmap.md) | Development direction and planned milestones |
| [Python](python/index.md) | The Python research layer (Phase 7) |

Getting started, building and contributing live in the repository's [README](https://github.com/JackBehindWood/oryx/blob/main/README.md) and [CONTRIBUTING](https://github.com/JackBehindWood/oryx/blob/main/CONTRIBUTING.md).

## Working on these docs

```bash
uv run build docs serve   # live preview
uv run build docs build   # strict build into site/ (fails on broken links)
```
