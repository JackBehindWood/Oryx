# vendor/

Drop header-only third-party libraries here, one per subdirectory (typically
a git submodule — see `tests/vendor/doctest` for the existing example), then
call `useVendorHeader("<lib>")` from this project's `premake5.lua`.

Compiled (non-header-only) libraries — e.g. `spdlog/` — instead get a
generated build script at `premake/<lib>.lua` (a sibling directory here,
never inside `<lib>/` itself, which stays a pristine upstream checkout).
Both flows are scaffolded by `uv run build vendor add` — see
`build_system/commands/vendor.py`.
