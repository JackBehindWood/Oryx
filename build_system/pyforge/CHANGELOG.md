# Changelog

## Unreleased

Extracted from Oryx's `build_system/` into a project-independent package. See the Oryx
repository's `docs/design/decision-log.md` for the design history up to this point.

### Phase 5 — build-system upgrades

- `[build] launcher = "ccache"` prepends a compiler launcher to the gmake toolchain.
- Premake is cached per version in the user cache; Linux arm64 (no release asset) falls back to `[premake] path` or a `premake5` on `PATH`.

### Phase 6 — fetched dependency sources

- `archive` (sha256-verified `.tar.gz`/`.zip`), `file` (single file), `git` (shallow clone, commit-verified) and `system` (`pkg-config` or explicit paths) dependency sources, stored in a shared user cache with atomic temp+rename writes.
- `forge deps sync` fetches in parallel; `forge deps clean-cache [--unused]` prunes the cache using a cross-project pin manifest.
- Global `--offline` fails fast, listing what would be fetched.
- `forge deps add` runs an interactive source picker in a TTY.

### Phase 7 — hardening

- Downloads accept only `http(s)` URLs; option-like `git` and `pkg-config` arguments are refused.
- Pin-manifest updates are serialised across processes with a file lock.
- A synthetic dummy plugin exercises the plugin hooks in pyforge's own tests.
- CI checks that `build_system/pyforge` extracts cleanly with `git subtree split` and imports nothing outside itself.

### Phase 8 — Windows

- The CLI, config, dependencies, Premake install and editor files work natively on Windows; building C++ there is still unsupported.
- Read-only files (cloned `.git` objects) are removed reliably, pin-manifest replacement retries on `PermissionError`, generated files use LF endings, subprocess output is decoded as UTF-8, and `forge run` exits 130 on Ctrl-C.
- The `tooling` CI job runs the full pyforge suite plus a `forge` smoke run on `windows-latest`.
