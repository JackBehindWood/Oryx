# Build tooling (pyforge)

The `forge` CLI drives Premake5 from `forge.toml`. It's project-independent — a package called
[`pyforge`](https://github.com/JackBehindWood/oryx/blob/main/build_system/pyforge/README.md),
extracted into `build_system/pyforge/` — extended for Oryx specifically by a plugin,
[`build_system/oryx/`](https://github.com/JackBehindWood/oryx/blob/main/build_system/oryx/).
See [`build_system/README.md`](https://github.com/JackBehindWood/oryx/blob/main/build_system/README.md)
for how the pieces fit together.

## Motto: "You only pay for what you use"

No `[docs]` table → no `docs` command group. No `[dependencies]` → no dependency checks before
`compile`. No plugins configured → pluggy is never imported. The interactive menu needs
`pyforge[menu]` (questionary); without it, a bare `forge` prints `--help` instead of crashing.
Every command module, and every optional extra, is imported only when the command that needs it
actually runs.

## CLI

```
forge                                   interactive menu (falls back to --help outside a TTY)
forge init [--yes] [--template app]     derive forge.toml from an existing premake5.lua, or scaffold a new project
forge configure | compile | all | clean
forge run [TARGET[:PRESET]] [-- args]   e.g. forge run oasis:bench, forge run -- --dashboard
forge test [SUITE…] [--list] [-- args]  e.g. forge test unit -- --test-case="*Vec3*"
forge config show | get KEY | set KEY VALUE | unset KEY [--local]
forge target add|remove|list
forge deps add|sync|update|status|remove
forge premake status|install|update [--version V]
forge docs build|serve|clean
forge editor vscode|vs2022
forge python stubs                      (Oryx plugin)
```

Global options (before the subcommand): `--profile debug|release|dist`, `--with OPTION` /
`--without OPTION` (toggle a `[options]` switch for this run), `-D KEY[=VALUE]` (straight to
Premake), `--verbose`, `--dry-run`, `--config PATH`.

## `forge.toml`

Only `[project] name` is required; every other table is optional with the defaults shown.
Unknown keys and wrong types fail immediately, with a "did you mean" suggestion.

```toml
[project]
name = "Oryx"
default-target = "oasis"         # `forge run` with no argument
forge-version = ">=0.2"          # checked at load, with an upgrade hint

[premake]
version = "5.0.0-beta8"
generator = "gmake"
path = ""                        # override the shared user-cache install, e.g. a Linux arm64 build

[build]
default-profile = "debug"
jobs = 0                         # 0 = all cores
dependencies-dir = "vendor"
fetch = "auto"                   # auto | ask | never: fetch missing required dependencies on configure

[options]                        # build switches → Premake flags; changing one wipes stale outputs
python = { default = true, off = "--no-python", help = "Embedded Python backend" }

[targets.oasis]
project = "Oasis"                # a Premake project; path and kind come from the export
presets.bench = ["--simulate=random,first-legal,100", "--benchmark"]

[tests]
project = "Tests"
suites.unit = "tests/unit"
suites.python = { dir = "tests/python", requires = ["python"] }

[dependencies]
spdlog = { kind = "static", include = "include", sources = "src" }

[docs]
tool = "mkdocs"

[plugins]
paths = ["build_system/oryx"]

[tool.oryx]                      # plugin-owned table, validated by the plugin
stubs-dir = "OryxPython/stubs"
```

`forge.local.toml` (gitignored, per-developer) holds editor/debugger choice and optional
overrides of `[build]`/`[options]`.

## Dependencies

A `[dependencies.<name>]` entry's `source` says how its files get onto disk:

| `source` | Files come from | Needs |
|---|---|---|
| `submodule` (default) | a git submodule, tracked by the superproject | — |
| `local` | files you put in place yourself; forge never fetches, updates, or deletes them | — |
| `git` | a shallow clone in the shared user cache, pinned to a commit | `url`, `commit` (and `rev`) |
| `archive` | a `.tar.gz`/`.zip` extracted into the shared user cache (a single top-level folder is flattened) | `url`, `sha256` |
| `file` | one file (e.g. a single header) downloaded as-is into the shared user cache | `url`, `sha256` |
| `archive` + `kind = "tool"` | an executable from a per-host archive: `binary` (path inside the archive) and `[dependencies.NAME.platforms.<os>-<arch>]` tables (`macos-aarch64`, `linux-x86_64`, ...) each with `url` and `sha256`; Premake sees the resolved `binary` | `binary`, `platforms` |
| `system` | a library already installed on the machine, found through `pkg-config` | `pkg-config`, or `include` (and `lib`) paths |

`kind = "static"` builds it from `sources`; header-only entries just contribute an include path.
`requires` lists `[options]` that must be on (`!name` for off) — an entry whose requirements aren't
met is neither fetched nor built.

`forge deps add NAME (--local | --submodule URL | --archive URL | --file URL | --git URL [--rev R] |
--system PKG)` detects the layout (an `include/` folder, a `src/` folder) and writes the entry;
`--archive`, `--file` and `--git` fetch once and record the `sha256` / resolved `commit` for you.
`forge deps sync` fetches everything a build currently needs; `forge deps status` lists every
entry's state, plus folders under `dependencies-dir` that no entry refers to; `forge deps update`
moves a `git` dependency to a new `--rev` and rewrites its `commit`.

Fetched sources (`git`, `archive`, `file`) share one cache, `<user cache>/deps/<name>/<pin>`, across
every project and worktree. `forge deps clean-cache` deletes it; `--unused` deletes only entries no
forge project on the machine still pins. A `system` dependency has no folder: consume it from a
project's `premake5.lua` with `forge.use("name")`, which adds its compiler and linker flags.

`forge --offline <command>` never touches the network: a missing `git`/`archive`/`file`
dependency (or an uncached Premake) fails immediately, listing everything that is missing.

### Walkthrough: glad (`local`)

Generate glad with its web service or CLI and put the result at `vendor/glad` (your
`dependencies-dir`), with headers under `include/` and `glad.c` under `src/`. Then:

```bash
forge deps add glad --local
```

forge detects the layout and writes:

```toml
[dependencies]
glad = { source = "local", kind = "static", include = "include", sources = "src" }
```

and from a project's `premake5.lua`, `includedirs { forge.include("glad") }` and `links { "glad" }`.

### Walkthrough: GLFW (`git`)

```bash
forge deps add glfw --git https://github.com/glfw/glfw.git --rev 3.4
```

forge shallow-clones tag `3.4` into the shared cache, detects its layout, and records both the tag
and the commit it resolved to:

```toml
[dependencies]
glfw = { source = "git", url = "https://github.com/glfw/glfw.git", rev = "3.4", commit = "<40-hex commit>", ... }
```

Later runs fetch that exact commit, and fail if the cache entry doesn't match it.

## Premake

Premake5 installs into a shared user cache (`~/.cache/pyforge/premake/<version>/` on Linux,
`~/Library/Caches/pyforge/premake/<version>/` on macOS, `%LOCALAPPDATA%\pyforge\premake\<version>\`
on Windows — `PYFORGE_CACHE` overrides the base directory), keyed by the pinned version, so every
project and worktree on the machine shares one download. `[premake] path` in `forge.toml`
overrides this with a build of your own (e.g. Linux arm64, which has no official release asset).

Every download is checksum-verified: against GitHub's own release-asset digest, and — for
pyforge's own pinned default version — against a hash pyforge ships independently of the GitHub
API response. `forge premake status|install|update [--version V]` manage the install directly;
`update` also rewrites `forge.toml`'s `[premake] version`.

## Windows

pyforge itself runs natively on Windows: the CLI, `forge.toml` handling, dependency fetching,
the Premake install (`premake5.exe` from the `windows.zip` asset into `%LOCALAPPDATA%\pyforge`)
and `forge configure`, and editor files. The `windows-latest` tooling CI job verifies this. Building
C++ with forge on Windows is unsupported: compiling is `make`-based, Python-enabled builds need
a shared libpython, and Oryx's C++ core does not build on Windows. See the pyforge README's
"Platform independence" section.

## Plugins

A plugin is a Python module with `@hookimpl`-decorated functions, loaded from a `[plugins]
paths` entry (a project-relative directory — see `build_system/oryx/` for a complete example).
Hooks: `forge_commands(app)` to mount new CLI commands, `forge_config_schema()` for a
`[tool.<name>]` table, `forge_premake_args(ctx)` to add Premake flags, `forge_pre_configure` /
`forge_post_compile` / `forge_post_clean(ctx)`, `forge_test_env(ctx, suite)` for suite-specific
environment variables, `forge_editor_contributions(ctx)`, and `forge_dependency_sources()` for a
new `[dependencies.*] source` kind. Plugins import only `pyforge.api` (`API_VERSION`), never
pyforge's internals directly.

## Working on pyforge itself

```bash
cd build_system/pyforge
uv sync --group test    # or: pip install -e ".[menu,plugins]" && pip install pytest
pytest                  # fast unit tests, plus a slow-marked real end-to-end run
```

`build_system/pyforge/` never imports or paths outside itself; it's `git subtree split
--prefix=build_system/pyforge`-ready.
