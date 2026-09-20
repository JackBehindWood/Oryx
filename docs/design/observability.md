# Observability

Observability should be optional.

A simple strategy should be able to operate with essentially:

```text
Action choose(state)
```

while an instrumented strategy might provide:

```text
Decision
├── action
├── probabilities
├── values
├── search statistics
└── diagnostics
```

Instrumentation should not force every strategy into a common diagnostic model.

Algorithm-specific diagnostics should be possible.
