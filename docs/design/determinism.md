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

## Experiment seeding (Phase 8)

Phase 8 answers the open questions above for experiments:

* **Who owns the RNG:** each component owns its own `oryx::Random`; the experiment layer only chooses seeds.
* **How seeds are assigned:** `derive_seed(master, key, role)` hashes the matchup key (FNV-1a-64) and mixes in the master seed, the role (game, strategy seat, experiment) and the repeat with a splitmix64-style finaliser. The key is content-based (a label or a canonical matchup string), so seeds do not depend on execution order or on which other matchups exist.
* **Parallel simulations:** a trial's seeds depend only on its key and repeat, so any executor, order or resume point yields identical results.
* **Independent streams:** one stream per (role, seat), so adding a chance stream to a game later does not shift strategy streams.
* **Reaching components:** by the `"seed"` Int param convention; an entry whose schema declares no `seed` is assumed deterministic. Injected seeds are masked to 63 bits so they are valid non-negative Int params for script strategies too.
* **Reproducing:** result metadata records the master seed, spec hash, Oryx version and build information; `rerun` must reproduce identical counts.
