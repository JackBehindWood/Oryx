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

Documentation follows the same rule: `forge docs` wraps MkDocs (configured in `mkdocs.yml`) rather than reimplementing any of it.

Premake itself defines three sibling projects: `Oryx` (the engine, a static
library), `tests` (the engine's own test suite), and `Oasis` (a console
application that links `Oryx`). `Oasis` exists so that games, demos, and
experiments built on top of the engine have a home that is not the engine
itself — the same reasoning behind keeping the CLI a thin orchestration layer
rather than absorbing Premake's responsibilities.

## Source Layout: Public API and Backends

Inside the `Oryx` project, the folder a file lives in says who may depend on it:

```text
Oryx/
├── src/        public API: the only include path consumers get
├── backends/   private: optional and platform-specific implementations
│   ├── Python/     (first backend, Phase 7)
│   ├── MacOS/      (platform-specific code, when needed)
│   └── Metal/, GLFW/ ...   (graphics backends, Phase 10)
└── vendor/     every vendored third-party library, backends included
```

* `Oryx/src` is the public API. Consumers such as `Oasis`, `tests`, and
  third-party projects see only this include path.
* `Oryx/backends/<Name>/` is private implementation code. Public headers never
  include from it; a backend implements an interface declared in `Oryx/src`
  (for example, a Python runtime implementing a language-agnostic
  `IScriptRuntime`) and is selected through the existing self-registering
  factory mechanism ([Architecture §10](../architecture.md#10-extension-model)), so consumers never name a backend type.
* Backends are compiled into the `Oryx` library by Premake: a build option
  turns an optional backend (such as Python) on or off, and an OS filter
  selects platform-specific ones. Turning one off leaves the rest of `Oryx`
  untouched.
* All vendored libraries live in `Oryx/vendor`, never in a per-backend vendor
  folder.
* The word "platform" in this project already means operating-system support
  (`PlatformDetection.h`, [Decision Log](decision-log.md)), which is why optional
  dependency-bound code such as Python is filed under `backends/`, not `platform/`.

`Oryx/backends/` holds the Python backend so far. See the [Decision Log](decision-log.md) ("Public API vs private backends").

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
