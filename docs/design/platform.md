# Platform and Tooling

## Serialization

Serialization may eventually be useful for:

* Saving game states
* Experiment configuration
* Results
* Replaying decisions
* Debugging
* Reproducibility

However, serialization should not become a mandatory requirement for every core object until a concrete use case justifies it.

## Graphics

Graphics should consume game state rather than own game logic.

The renderer should not determine whether a move is legal.

Conceptually:

```text
Game
 │
 └── State ──► Renderer
```

rather than:

```text
Renderer ──► Game rules
```

This allows the same game to run:

* Headlessly
* In a desktop UI
* In an educational visualisation
* In automated tests
* In a server environment

Phase 3's `TicTacToeBoard` (in `Oasis`) is a concrete class, not yet behind
a shared `IBoard` interface — with one game and one renderer, an interface
has no second implementation to justify it. `IBoard` is extracted once
Phase 10 Graphics needs to swap in a graphical renderer polymorphically
([Architecture §8](../architecture.md#8-graphics)), following the same don't-build-it-before-it's-needed
reasoning as the `Registry<T>` timing decision ([Decision Log](decision-log.md)).

## Build System

Premake5 remains the C++ build/project-generation system.

The Python build CLI exists to provide developer ergonomics around it.

The intended separation is:

```text
build_system CLI
   │
   ├── configuration
   ├── Premake invocation
   ├── compilation workflow
   ├── testing
   ├── benchmarks
   ├── documentation
   └── development commands
          │
          ▼
       Premake5
          │
          ▼
     C++ toolchain
```

The Python CLI should orchestrate rather than duplicate the responsibilities of Premake.

Documentation follows the same rule: `build docs` wraps MkDocs (configured in `mkdocs.yml`) rather than reimplementing any of it.

Premake itself defines three sibling projects: `Oryx` (the engine, a static
library), `tests` (the engine's own test suite), and `Oasis` (a console
application that links `Oryx`). `Oasis` exists so that games, demos, and
experiments built on top of the engine have a home that is not the engine
itself — the same reasoning behind keeping the CLI a thin orchestration layer
rather than absorbing Premake's responsibilities.

## Documentation as a Design Tool

Documentation is not only for users.

The architecture and design documents should expose unresolved decisions before they become accidental implementation decisions.

When an important question is unresolved, it is preferable to record:

```text
Decision: Open
Options: A / B / C
Reason unresolved: ...
Next step: ...
```

rather than silently choosing an architecture in code.
