# vendor/

Drop header-only third-party libraries here, one per subdirectory (typically
a git submodule — see `tests/vendor/doctest` for the existing example), then
call `useVendorHeader("<lib>")` from this project's `premake5.lua`.

Compiled (non-header-only) libraries — e.g. `spdlog/` — instead get a
generated build script at `premake/<lib>.lua` (a sibling directory here,
never inside `<lib>/` itself, which stays a pristine upstream checkout).
Both flows are scaffolded by `uv run build vendor add` — see
`build_system/commands/vendor.py`.

## Pinned versions

`pybind11/` is a git submodule pinned to an untagged upstream commit (`v3.0.2-82-g97bf890d`), not a release tag. It is the snapshot the Python backend was developed and tested against (CPython 3.11 and 3.14); move it to a release tag once one contains that commit, then re-run the Python-on and `--no-python` builds.
