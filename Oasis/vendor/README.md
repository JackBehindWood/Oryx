# vendor/

Drop header-only third-party libraries here, one per subdirectory (typically
a git submodule — see `tests/vendor/doctest` for the existing example), then
call `useVendorHeader("<lib>")` from this project's `premake5.lua`.
