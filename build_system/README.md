# Oryx Engine Build System

A Python-based CLI build automation system for the Oryx Engine. Built on top of **Typer**, **Rich**, **Questionary**, and **Premake5**, it manages platform-specific toolchains, configurations, compilation, and testing workflows.

---

**Features**

* **Interactive Menu**: Run `uv run forge` with no arguments in a terminal for an arrow-key command menu — no need to memorize subcommand names.
* **Auto-Registering Commands**: Command groups under `build_system/commands/` are discovered automatically; adding one requires no edits to `main.py` or `interactive.py`. See "Extending the CLI" below.
* **TOML Configuration**: Shared settings live in `forge.toml` (git-committed, found by walking up from the current directory, so `forge` works from any subdirectory); per-developer preferences live in `forge.local.toml` (gitignored) — see "Personal preferences" below.
* **Isolated Dependency Management**: Automatically downloads and extracts the required Premake5 release locally using `urllib`, `tarfile`, and `zipfile` utilities, automatically setting system execution permissions.
* **Cross-Platform Output Structuring**: Dynamically constructs target binary paths matching Premake conventions (`<Config>-<OS>-<Arch>`) based on host architecture and OS detection.
* **Configurable CLI Profiles**: Switch between `debug`, `release`, and `dist` build configurations via global context flags.
* **Integrated Workflow Execution**: Run complete sequential pipelines (configure, compile, and test) with single-command convenience.
* **Optional IDE Integration**: `forge config init --ide vscode` generates/merges `.vscode/{tasks,settings,c_cpp_properties,launch}.json`, including a single-button build-and-debug flow with one Debug/Release/Dist configuration each, selectable from VS Code's Run & Debug dropdown. `forge config init --ide visual_studio` instead generates a Visual Studio solution via Premake's own `vs2022` action. Neither is required — the CLI itself never imports IDE-specific code unless you ask for it.
* **Generic Vendoring Convention**: Header-only third-party libraries (git submodules) live under `<project>/vendor/<lib>/`, wired up with one `useVendorHeader(...)` call in Premake and auto-discovered on the Python side — see "Vendoring third-party libraries" below.
* **CLI Command Script**: Installs directly as the `build` executable via standard package entry points (`pyproject.toml`).

---

**Installation & Requirements**

* **Python**: `>=3.11`
* **Build Backend**: Hatchling
* **Git submodules**: `tests/vendor/doctest` (the test framework) is a git submodule.
  Run `git submodule update --init --recursive` after cloning — any command that
  needs vendored headers checks for it and fails with this exact command if it's missing.

### Using `uv` (Recommended)

Run the CLI instantly without explicit installation:

```bash
uv run forge [COMMAND]

```

Or install the package in editable mode within your environment:

```bash
uv pip install -e .

```

### Using Standard `pip`

```bash
pip install -e .

```

---

**CLI Overview**

Once installed, invoke the CLI using the `build` command (or via `python -m build_system.main`):

```bash
build [GLOBAL OPTIONS] COMMAND [ARGS]...

```

Running `build` with **no command** opens an interactive arrow-key menu (in a real terminal); in a non-interactive context (CI, pipes) it prints help instead.

**Global Options**

| Option | Short | Description |
| --- | --- | --- |
| `--config PATH` | `-c` | Path to the project's `forge.toml` (default: the nearest one at or above the current directory). |
| `--profile [debug\|release\|dist]` | `-p` | Active build configuration profile (default: `[build] default-profile`). |
| `--verbose` | `-v` | Show full subprocess output and the underlying commands being run. |
| `--dry-run` | | Print the command that would run without executing it. |
| `--with OPTION` / `--without OPTION` | | Turn a `forge.toml` `[options]` switch on or off for this run (repeatable). Each option maps to its `on`/`off` Premake flag. Python is on by default; `configure` reads the interpreter's paths from `sysconfig` and passes them to Premake. |
| `-D KEY[=VALUE]` | | Pass `--KEY[=VALUE]` straight to Premake (repeatable). |

Changing any of these (or the interpreter) changes the options hash in `build/forge/stamp.json`; the next
`configure` or `compile` then regenerates the build files and wipes `build/bin` and `build/bin-int`, since
Make would not rebuild objects whose flags changed. `--no-python` and `--sanitize` remain as hidden aliases
of `--without python` and `--with sanitize`.

---

**Command Reference**

**`config`**

Manages build settings and local configurations.

| Command | Description |
| --- | --- |
| `config init` | Generates a minimal `forge.toml` in the current directory when none exists. |
| `config init --ide vscode [--debugger lldb\|cppdbg]` | Also generates/merges `.vscode/{tasks,settings,c_cpp_properties,launch}.json`, remembered in `forge.local.toml`. |
| `config init --ide visual_studio` | Generates a Visual Studio 2022 solution via `premake5 vs2022` — independent of `forge build compile`, which still uses `forge.toml`'s `[premake] generator` (default `gmake`). |
| `config init --no-remember` | One-shot `--ide`/`--debugger` override; doesn't touch `forge.local.toml`. |

**`build`**

Handles build lifecycle, Premake configuration, and binary compilation.

| Command | Description |
| --- | --- |
| `build configure` | Ensures local Premake5 binary exists and generates project build files. Records the compiled source files in `build/.sources`; when a source was removed since the last run it deletes that project's binaries so a stale archive member or executable cannot survive. |
| `build compile` | Compiles engine binaries for the targeted configuration profile. Runs `configure` first when a source file was added or removed since the last configure. |
| `build run` | Runs the compiled `Oasis` sandbox executable (`--game`, `--opponent`, `--simulate`, `--benchmark` are forwarded). |
| `build clean` | Removes the entire `build/` directory (binaries, object files, generated Makefiles, and `compile_commands.json`). |
| `build all` | Executes `configure`, `compile`, and unit test commands sequentially. |

**`test`**

Manages test execution suites.

| Command | Description |
| --- | --- |
| `test` / `test run` | Executes the compiled test binary for the active build profile (bare `test` runs it directly). |

---

**Configuration (`forge.toml`)**

Only `[project] name` is required; every other table is optional with the defaults shown.
The schema is strict: an unknown key or a wrong type fails with the file and key named,
plus a "did you mean" suggestion.

```toml
[project]
name = "Oryx"
default-target = "oasis"          # `forge build run` with no argument
forge-version = ">=0.2"           # checked at load

[premake]
version = "5.0.0-beta8"
generator = "gmake"

[build]
default-profile = "debug"         # debug | release | dist
jobs = 0                          # 0 = all cores
dependencies-dir = "vendor"
fetch = "auto"                    # auto | ask | never

[options]                         # build switches → Premake flags
python = { default = true, off = "--no-python", help = "Embedded Python backend" }

[targets.oasis]
project = "Oasis"                 # a Premake project name

[tests]
project = "Tests"

[docs]
tool = "mkdocs"                   # config/site default to mkdocs.yml / site

[tool.oryx]                       # project-owned tables, not validated by forge
stubs-dir = "OryxPython/stubs"
```

Closed value sets (profile, fetch mode, dependency kind, debugger) are `StrEnum`s in
`build_system/config/schema.py`. Open sets that plugins will extend (generators,
dependency sources, editors, doc tools) are `Choices` registries with `register()`.

---

**Personal preferences (`forge.local.toml`)**

`forge.toml` is shared and git-committed; editor choice, debugger and personal build
overrides live in the **gitignored** `forge.local.toml` next to it:

```toml
[editor]
kind = "vscode"     # "vscode" | "visual_studio" | "none"
debugger = "lldb"   # "lldb" | "cppdbg"

[build]             # optional overrides of the shared [build]
jobs = 6

[options]           # optional overrides of the shared [options] defaults
python = false
```

An older `oryx.local.toml` (with `[ide]` instead of `[editor]`) is still read when no
`forge.local.toml` exists; forge prints a rename hint and never modifies or deletes it.

`forge config init --ide <kind> [--debugger <name>]` resolves against whatever
is already saved here (so a bare `forge config init` re-run reuses your last
choice instead of resetting to `none`), then saves the result back unless you
pass `--no-remember`.

---

**Usage Examples**

```bash
# Open the interactive menu
uv run forge

# Initialize build configuration file (and VS Code integration)
uv run forge config init --ide vscode

# Configure and compile in Release mode
uv run forge --profile release build configure
uv run forge --profile release build compile

# Run the complete pipeline (configure, build, and test) in Debug mode
uv run forge build all

# Run the Oasis sandbox executable
uv run forge build run

# Preview what a command would do without running it
uv run forge --dry-run build compile

# Clean build artifacts
uv run forge build clean

```

---

**Extending the CLI**

Each command group is a self-contained module under `build_system/commands/`.
`build_system/main.py` and `build_system/interactive.py` auto-discover every
module in that directory via `pkgutil.iter_modules` — dropping in a new file
is enough, with **no edits needed elsewhere**. To add a new command group
(e.g. the `benchmark`/`experiment`/`docs`/`explain` commands from
`docs/roadmap.md`):

1. Create `build_system/commands/<name>.py` with its own `app = typer.Typer()`,
   a `GROUP_HELP` string, and `command = registry.make_group(app, group="<Name>")`.
2. Decorate each command function with `@command(name="...", label="...")`.
   `name` is the Typer subcommand name; `label` is what shows up in the
   interactive menu. That's the entire registration — no `MENU_ENTRIES` list,
   no `GROUPS` entry, no `add_typer()` call.

```python
import typer
from build_system import registry

app = typer.Typer()
GROUP_HELP = "One-line description shown in --help"
command = registry.make_group(app, group="Benchmark")

@command(name="run", label="Run — execute the benchmark suite")
def run(ctx: typer.Context):
    ...
```

Menu groups display in alphabetical discovery order by default; set a
module-level `GROUP_ORDER = <int>` to pin a specific position.

`@command(...)` accepts two extra flags:
* `hidden=True` — keep a command callable from the CLI but leave it out of
  the interactive menu (for internal/plumbing commands).
* `requires_vendor=True` — run `build_system.vendor.ensure_vendor_dirs()`
  before the command's body (skipped under `--dry-run`), aborting with the
  usual "run `git submodule update --init --recursive`" message if a
  vendored submodule is missing. `build.py`'s `compile` command uses this
  instead of a hand-written check.

Commands read shared state via `ctx.obj`, a `RunContext` (`build_system/config.py`)
carrying the loaded `BuildConfig`, `--verbose`, and `--dry-run`. The
interactive menu also auto-prompts for any extra `typer.Option` parameters a
command declares (`inspect.signature` + the option's own type/help text) —
`bool` renders as a confirm, `Literal[...]` as a select, anything else as free
text — so a well-typed command gets a sensible interactive prompt without any
special-casing in `interactive.py`.

Compile-command construction for a given Premake generator lives in
`build_system/setup/generators.py` — adding a new generator means registering
a new builder function there, not editing `commands/build.py`.

---

**VS Code integration details**

Generated `.vscode/*` files live under `build_system/vscode/` as one module
per file (`tasks.py`, `settings.py`, `c_cpp_properties.py`, `launch.py`),
sharing a small JSON-merge-by-key helper in `build_system/utils/json_files.py`
so re-running `config init --ide vscode` replaces only the entries this CLI
owns and leaves anything else in those files untouched. `write_all()` in
`build_system/vscode/__init__.py` is the entry point `commands/config.py`
calls; the `vscode` package itself is never imported unless `--ide vscode`
is actually requested.

`tasks.py` generates one `Compile (<Profile>)` task per build profile
(Debug/Release/Dist); `launch.py` generates a matching debug configuration
per profile, each with that task as its `preLaunchTask` — so VS Code's
native Run & Debug dropdown gives you a single button that builds the right
profile and starts debugging, the closest match to Visual Studio's
configuration dropdown without needing a custom extension. The default
debugger is `lldb` (the [CodeLLDB](https://marketplace.visualstudio.com/items?itemName=vadimcn.vscode-lldb)
extension); pass `--debugger cppdbg` for [Microsoft C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools)
instead — the only place this branches is the small `DEBUGGER_TYPES` /
`DEBUGGER_EXTRA_KEYS` dict pair at the top of `vscode/launch.py`.

`c_cpp_properties.py` points VS Code's `compileCommands` at
`build/compile_commands.json`, which `build_system/compile_commands.py`
regenerates on every `forge build configure` by dry-running (`make -n -B`)
each Premake-generated `build/*.make` file and capturing the real, fully-resolved
per-file compiler invocations — so IntelliSense stays accurate per project
(and covers new projects automatically) without any hand-maintained include
list. The `includePath`/`defines` still baked into `c_cpp_properties.py`
are only a fallback for before `configure` has ever run.

---

**Vendoring third-party libraries**

Header-only third-party libraries (typically git submodules) live under
`<project>/vendor/<lib>/` — see `tests/vendor/doctest` for the existing
example, and the empty `Oryx/vendor/`, `Oasis/vendor/` directories ready for
future ones. Two pieces make this a one-line convention rather than a
bespoke, hand-wired path per library:

* **Premake**: `premake/vendor.lua`'s `useVendorHeader(libName, headerSubdir)`
  sets `includedirs` to the library's real header directory and `removefiles`
  to exclude the rest of the submodule (its own tests/examples/build files)
  from compilation. `tests/premake5.lua` calls `useVendorHeader("doctest", "doctest")`.
  `premake/common.lua` similarly factors out the `language`/`cppdialect`/
  `staticruntime`/`targetdir`/`objdir` lines every project repeats, via
  `useOryxProjectDefaults()`. Both are included once from the root
  `premake5.lua` and are tracked in git — only the downloaded Premake5
  binary under `premake/bin/` is gitignored.
* **Python**: `build_system/vendor.py` scans `Oryx/vendor/*`, `Oasis/vendor/*`,
  and `tests/vendor/*` on disk — `missing_vendor_dirs()` (backing the
  `requires_vendor=True` command flag above) flags submodules that haven't
  been checked out, and `vendor_include_paths()` feeds the VS Code
  IntelliSense fallback paths and search excludes generically, instead of a
  hardcoded doctest-specific path. With Python on, the fallback also adds
  `Oryx/backends/Python` (backend + the `oryx` package), pybind11, the
  interpreter's `Python.h` directory and the baked `OX_PYTHON_*` defines.

To vendor a new library: add it as a git submodule under `<project>/vendor/<lib>/`,
add `useVendorHeader("<lib>")` (with a second argument if its header sits in
a subdirectory, as doctest's does) to that project's `premake5.lua`, and
nothing else — the Python side picks it up automatically.
