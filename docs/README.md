# Oryx documentation

Source for the Oryx documentation site (MkDocs Material). `README.md` and `CONTRIBUTING.md` stay at the repository root; everything else lives here.

## Layout

| Path | What belongs here |
| ---- | ----------------- |
| `index.md` | Site landing page |
| `architecture.md` | **What** components exist and how they relate |
| `design/` | **Why** we made particular technical choices, split by topic; `design/decision-log.md` records what is settled and what is open |
| `roadmap.md` | Phases and planned milestones |
| `python/` | The Python research layer (Phase 7) |

Check `design/decision-log.md` and the open-questions list in `architecture.md` before writing about an undecided topic, and record new decisions in the decision log rather than in code comments.

## Working on the docs

```bash
uv run forge docs serve   # live preview at http://localhost:8000
uv run forge docs build   # strict build into site/; fails on broken links and anchors
uv run forge docs clean   # remove site/
```

The docs toolchain is the optional `docs` dependency group in `pyproject.toml`; `uv run forge docs ...` installs it on demand. CI runs the strict build on every pull request. Pushes to `main` also publish the site to GitHub Pages (`.github/workflows/docs.yml`); this needs Settings → Pages → Source set to "GitHub Actions" once.

## Conventions

* Add a new page to `nav:` in `mkdocs.yml`; pages missing from the nav still build, but won't be reachable from the sidebar.
* Link to other docs with relative links (`../architecture.md#3-core-components`). Anchors are checked by the strict build.
* Link to files outside `docs/` (source, `build_system/README.md`) with a full GitHub URL, because a relative link that leaves `docs/` fails the strict build.
* Reference docs from source comments by path (`docs/design/quality.md`), never by section number.
* This `README.md` is excluded from the site; it is for people browsing the repository.
