# Randomness and Determinism

## Randomness

Randomness is fundamental to many intended use cases.

The design should eventually distinguish between:

```text
Randomness source
        ↓
Game randomness
Strategy randomness
Experiment randomness
```

Reproducibility requires explicit control over random-number generation.

We should avoid hidden global random state.

The final design should answer:

* Who owns the RNG?
* How are seeds assigned?
* How are parallel simulations seeded?
* Can individual components have independent streams?
* How are random states reproduced?

These questions should be resolved before the simulation system becomes substantial.

As of the Phase 2 brainstorm, a standalone, seedable `oryx::Random` utility
(wrapping `std::mt19937_64`) has been added to `Oryx/Core`. It is not yet
wired into `IGame`/`IState` — Phase 2/3 have no chance nodes — but exists so
Phase 4's Random strategy, and later reproducibility work, have a single
reproducible source rather than each reaching for `<random>` independently.

Phase 4 resolves the strategy-level question: `RandomStrategy` owns its own
`oryx::Random` member, seeded via its own constructor argument. `IStrategy`
gained no seed/RNG parameter — seeding a batch's strategies is instead the
responsibility of whatever constructs them, i.e. the `Match`/batch-runner
level introduced in Phase 5 ([Architecture §5](../architecture.md#5-execution-model)), not the strategy
interface itself.

## Determinism and Reproducibility

The same experiment configuration should, where possible, be reproducible.

A reproducible experiment should ideally record:

```text
Game
Strategy
Parameters
Seed
Number of runs
Oryx version
Relevant environment information
Results
```

Parallel execution introduces additional challenges.

The system should distinguish between:

* Deterministic game logic
* Deterministic single-threaded execution
* Reproducible seeded simulations
* Bit-for-bit reproducibility

These are not necessarily equivalent.
