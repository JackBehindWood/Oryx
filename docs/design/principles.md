# Design Principles

## Design Goals

Oryx should optimise for:

1. Simplicity
2. Extensibility
3. Correctness
4. Testability
5. Reproducibility
6. Performance where it matters
7. Good developer experience
8. Good research experience
9. Educational value
10. Long-term API stability

Performance is important, but unnecessary complexity should not be introduced before a real performance requirement exists.

## Game != Strategy != Engine

This is the most important design constraint.

A game defines:

```text
What can happen?
```

A strategy defines:

```text
What should I do?
```

The engine defines:

```text
How do we execute, simulate, evaluate, and study it?
```

This separation should be maintained even when doing so requires slightly more explicit interfaces.

## Prefer Composition

Oryx should generally prefer composition over deep inheritance hierarchies.

For example, a strategy may be composed from:

```text
Search
+
Evaluation
+
Randomness
+
Policy
+
Observability
```

rather than requiring every algorithm to inherit from an increasingly large base class.

Inheritance should be used when there is a genuine stable "is-a" relationship and polymorphism provides clear value.

## Small Interfaces

Interfaces should expose only the capabilities actually required.

Avoid a universal interface such as:

```cpp
class Everything
{
    ...
};
```

A game that only requires legal-action generation should not be forced to implement unrelated concepts.

This is particularly important because Oryx may eventually support very different classes of games:

* Deterministic games
* Stochastic games
* Perfect-information games
* Imperfect-information games
* Simultaneous-action games
* Single-player environments
* Multi-agent environments

The abstractions should grow from demonstrated requirements.

`IGame`, `IState`, and `IStrategy` (see [Architecture §3](../architecture.md#3-core-components)) are examples
of this: each pure-virtual interface exposes only what callers actually
need, using an `I`-prefix reserved specifically for fully pure-virtual
interfaces (see [Naming Conventions](cpp-api.md#naming-conventions)).

## Design Review Principle

No major abstraction should be introduced simply because it appears useful in theory.

Before adding one, ask:

1. What concrete problem does it solve?
2. Is the problem recurring?
3. Can composition solve it more simply?
4. Does it improve or complicate the public API?
5. Does it preserve Game/Strategy/Engine separation?
6. Does it help both C++ and Python users where appropriate?
7. Can it be tested independently?
8. Does the project actually need it now?

If the answer is unclear, defer the abstraction.
