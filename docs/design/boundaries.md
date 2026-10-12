# Boundaries

Layering rules live in code, in the Oryx plugin ([`build_system/oryx/boundaries.py`](https://github.com/JackBehindWood/Oryx/blob/main/build_system/oryx/boundaries.py)), and are enforced by `forge boundaries check --artifacts` (CI runs it). The engine is generic and lives in pyforge; the rules are Oryx's.

## Declaring the tree

Parts of the tree are declared once as objects. Rules take those objects, never path strings, so a typo is a `NameError` and a part that matches nothing fails the check.

| Declare | Meaning |
| --- | --- |
| `spec.module("Core")`, `spec.modules("Core", "Game")` | A directory and everything under it. Takes arguments or a list. |
| `spec.discover(root).Interface.GUI` | Every directory as an attribute; a typo lists the real ones. Opt-in, mixes with `module`. |
| `spec.files("name", "Assets/**/Gpu*", "{Null,Metal}*.{h,hpp}")` | Globs: `*`, `**`, `?`, `{a,b}`. A glob without `/` matches a file name at any depth. |
| `spec.pattern("name", regex)` | A regex, for what a glob can't say. |
| `spec.group("graphics", a, b, c)` | Several parts treated as one. |

## Writing rules

| Rule | Meaning |
| --- | --- |
| `a.must_not_include(b, c, because=, unless=)` | `a` must not include `b` or `c` (except from `unless`). |
| `b.only_included_by(a)` | Only `a` may include `b`. |
| `b.must_not_be_included(unless=)` | Nothing may include `b` (except files in `unless`). |
| `spec.isolate([a, b], from_=[x, y])` | One rule for a whole group. |
| `spec.peers(a, b)` | None may include another. |
| `spec.above(top, *lower)` | `lower` must not include `top`. |
| `spec.layers(a, b, c)` | Each layer includes only itself and earlier ones. |
| `spec.waive(a, b, because=, expires="2026-12-31")` | Park a known violation; it becomes an error after the date and is reported when stale. |

## Messages

A violation prints a sentence generated from the rule and the real modules involved, with the rule's `because`:

`Oryx/Assets/ImageAsset.h:3: Assets must not include Core/Window.h (CPU assets stay headless)`

A rule's `message=` replaces it. `{default}` inside the template extends it instead: `message="{default}. See docs/design/graphics.md"`. `BoundarySpec(message=...)` sets one for every rule. Placeholders: `{default} {because} {source} {target} {file} {line} {path} {raw} {rule}`; an unknown name stays as written. `message` may also be a function `(include, rule) -> str`.

## Built libraries

`spec.library(path, when_off="graphics")` checks a built artifact; the options apply to every rule on it, and a rule can give its own. `when_on` and `when_off` take an option name, a handle from `spec.optional("graphics", graphics, because=...)` (its reason becomes the default `because`), or a list mixing both.

- `must_not_export(selection)`: nothing declared only by those modules may be defined. The forbidden names are **derived** from the source, so the check cannot drift.
- `must_export(selection)`: the module must really be in the build.
- `spec.symbols("python", "oryx::python", "pybind11", prefixes=["Py", "_Py"])` names symbols by namespace or prefix, for code outside the scanned tree. Pass it to `must_not_export`, `must_not_import` or `spec.optional`.
- `must_not_import("*glfw*")` and `must_not_export_symbols("oryx::Graphics::*")`: raw globs over undefined and defined symbols, the escape hatch.

Each artifact is read once per check, however many rules apply.

## More than one language

A project that mixes languages holds one `BoundarySpec` per language in a `BoundarySuite`; the `forge_boundaries` hook may return a spec, a suite or a list.

```python
suite = BoundarySuite()
native = suite.new("cpp", root="Oryx/src", base="Oryx", namespace="oryx")
tooling = suite.new("python", root="build_system")
```

`forge boundaries check` runs every spec and prefixes each violation with its spec's name when there is more than one. `--format json` and, inside GitHub Actions, `::error` annotations carry rule, file and line.

## Joining specs

`spec.merge(other, ...)` adds everything other specs declare (parts, rules, waivers, libraries) to `spec` and returns it; `BoundarySpec.joined(a, b, name=)` builds a new spec instead. The specs must scan the same root and base in the same language; a part declared in both must be identical, and identical rules are not duplicated. Use it to let several plugins or files each contribute rules for one tree. A part taken from another spec still adds rules to that spec through its own methods, so pass it to the merged spec's `forbid(...)` instead, or declare the rules before merging.

## Adding a language or artifact tool

Both are registries, so a plugin adds one by calling a function at import time:

- `register_scanner(name, suffixes, imports, resolve=None, declares=None)`: `imports(text)` lists what a file depends on, `resolve` maps an import to the path it names, `declares(text)` lists the names a file declares (needed for `must_not_export`). `cpp` and `python` are built in.
- `register_artifact_tool(name, defined, undefined=None)`: how to list the symbols an artifact defines and needs. `nm` is built in.
