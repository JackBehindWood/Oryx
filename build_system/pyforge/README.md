# pyforge

**You only pay for what you use.**

pyforge is a project-independent CLI (`forge`) that drives [Premake5](https://premake.github.io/)
from a single `forge.toml`: configure, compile, run, and test any C/C++ project, extended by
plugins for project-specific behaviour. It isn't tied to any particular engine or project —
Oryx (the project this package was extracted from) uses it through a plugin
(`build_system/oryx/`) that sits entirely outside this package.

That motto governs every design choice here, not just the marketing copy:

- **Commands appear only when configured.** No `[docs]` table in `forge.toml` → no `docs`
  command group. No `[dependencies]` → no dependency checks before `compile`. No plugins →
  pluggy is never imported.
- **Imports are paid at use.** Core install is `typer` + `rich` only. The interactive menu needs
  `pyforge[menu]` (questionary); without it, a bare `forge` degrades to printing `--help` instead
  of crashing. Plugin loading needs `pyforge[plugins]` (pluggy); without it, a project with no
  `[plugins] paths` configured works exactly the same as with it installed — only a project that
  actually sets `[plugins] paths` sees an install hint. Premake's own download machinery
  (`urllib`, a progress bar) is imported only by the commands that actually download something.
- **Work is paid at change.** Premake re-runs only when a stat-based freshness stamp says a
  source file, `forge.toml`, or a `.lua` script actually changed. Build-option changes wipe
  stale outputs only when the resolved option set's hash changes.
- **Runtime cost is zero where possible.** `forge run` `exec`s into the target on POSIX; forge's
  own Python process is gone while your program runs. Build and test output streams through a
  bounded buffer instead of being held in memory.
- **Config is paid at need.** Every `forge.toml` table beyond `[project]` is optional, with
  sensible defaults; `forge init` writes only what differs from them.

## Install

```bash
pip install -e "build_system/pyforge[menu,plugins]"   # from an Oryx-style checkout
# or, once published:
pip install "pyforge[menu,plugins]"
```

`menu` and `plugins` are optional; the core install is `forge configure/compile/run/test` plus
`forge.toml` editing (`config`/`target`/`deps`), no interactive menu and no plugin loading.

## Quickstart

```bash
mkdir myapp && cd myapp
forge init --template app     # scaffolds premake5.lua, src/main.cpp, forge.toml
forge all                     # configure, compile, test
forge run                     # build/run the executable
```

For an existing Premake project, `forge init` (no `--template`) derives `forge.toml`'s
`[targets]` from a real Premake export instead of scaffolding new files.

## CLI

```
forge                                   interactive menu (falls back to --help outside a TTY)
forge init [--yes] [--template app]     scaffold, or derive from an existing premake5.lua
forge configure | compile | all | clean
forge run [TARGET[:PRESET]] [-- args]
forge test [SUITE…] [--list] [-- args]
forge config show | get KEY | set KEY VALUE | unset KEY [--local]
forge target add|remove|list
forge deps add|sync|update|status|remove
forge premake status|install|update [--version V]
forge docs build|serve|clean
forge editor vscode|vs2022
global: --profile, --with X, --without X, -D KEY=VALUE, --verbose, --dry-run, --config
```

See [`docs/tooling.md`](../../docs/tooling.md) for the full `forge.toml` schema and a walkthrough
of dependencies, presets, and the plugin hooks.

## Development

```bash
cd build_system/pyforge
uv sync --group test        # or: pip install -e ".[menu,plugins]" && pip install pytest
pytest                       # includes a slow-marked, real end-to-end test (tests/fixtures/hello)
```

`tests/` never imports or references anything outside `build_system/pyforge/` — this directory
is a self-contained project, extractable on its own with:

```bash
git subtree split --prefix=build_system/pyforge -b pyforge-standalone
```

## License

Apache-2.0, see [LICENSE](LICENSE).
