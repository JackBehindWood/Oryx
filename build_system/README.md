# build_system

Oryx's build tooling, split into two independent pieces:

- **[`pyforge/`](pyforge/README.md)** — the project-independent CLI (`forge`) and its own
  `pyproject.toml`, `src/`, and `tests/`. It knows nothing about Oryx specifically; everything
  it does is driven by `forge.toml` and Premake's own export. This is the package that would be
  extracted with `git subtree split --prefix=build_system/pyforge` if it ever became a separate
  repository. Read its own README for the CLI reference and the "you only pay for what you use"
  design motto that governs it.
- **[`oryx/`](oryx/)** — the Oryx-specific pyforge plugin: Python build/embedding options
  (`--python-*` Premake args, `PythonConfig.h`, the `.pth` file, `forge python stubs`), and the
  `[tool.oryx]` `forge.toml` schema. Loaded via `forge.toml`'s `[plugins] paths =
  ["build_system/oryx"]`; nothing in `pyforge/` imports from here.

See [`docs/tooling.md`](../docs/tooling.md) for how the two fit together (the plugin hook
surface, `forge.toml`'s full schema) and [`CONTRIBUTING.md`](../CONTRIBUTING.md) for the uv and
pip setup instructions.
