# Python API

## Python API Design

Python should not simply mirror C++.

A Python user should be able to work at a higher conceptual level.

For example:

```python
experiment = Experiment(
    game=game,
    strategies=[random_strategy, mcts_strategy],
    games=10_000,
)

results = experiment.run()
```

This is illustrative rather than a proposed final API.

The actual API should be designed after the C++ core abstractions are clearer.

## Bindings

pybind11 is the current intended binding technology.

The binding layer should act as an API boundary rather than exposing every internal C++ type.

Internal C++ implementation details should remain internal where possible.

This reduces Python API churn and allows the C++ implementation to evolve independently.
