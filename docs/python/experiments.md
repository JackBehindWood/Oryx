# Experiments

`oryx.Experiment` and `oryx.Tournament` describe many matches as data, run them with reproducible seeds, and keep the results. The design is in the [Decision Log](../design/decision-log.md) and [Determinism](../design/determinism.md#experiment-seeding-phase-8); the C++ side lives in `Oryx/src/Oryx/Simulation/`.

## Describing an experiment

An experiment is a list of **matchups**: a game id, its parameters, and one strategy (id and parameters) per seat. Everything is named by registry id, so Python-defined games and strategies work once registered, and the same spec can be saved, diffed and rerun.

```python
oryx.Experiment(
    [{"game": "nim", "strategies": ["random", ("minimax", {})]}],
    matches=100, repeats=5, seed=42,
)
```

Two helpers expand into matchups:

* `Experiment.sweep(game, strategies, axes)`: the cartesian product of parameter values. A path is `game.<param>` or `seats.<index>.<param>`.
* `Tournament(game, pool, rotate_seats=True)`: every pair of the pool, seated both ways round.

`validate()` checks ids, parameters, seat counts and capabilities up front and `run()` calls it first.

## Seeds

Each trial's seeds come from `derive_seed(master, matchup key, role, seat, repeat)`. The key is the matchup's `label` or a description of its content, never its position, so adding, removing or reordering matchups leaves every other matchup's results unchanged. A strategy or game receives its seed through its `seed` parameter; set one explicitly to pin it. Two matchups with the same key are rejected: give one a `label`.

## Results

`run()` returns an `ExperimentResult`: the spec, build and seed metadata (Oryx version, git hash, profile, compiler, platform, spec hash, optional timestamp), and one metrics row per trial (`matches`, `wins/<seat>`, `draws`, `reward/<seat>`, `decisions`).

* `summary()`, `to_dict()`, `to_dataframe()` (tidy rows, needs pandas), and a notebook table.
* `series()`, `summarize()`, `compare()`: spread across repeats and a 95% interval for the difference between matchups.
* `cross_table()` and `ratings()` for two-seat matchups: scores against each opponent and Bradley-Terry strengths on the Elo scale.

## Storage

`save(directory)` writes `result.yaml` (schema version, spec, metadata, summary) and `trials.csv` (`matchup,repeat,metric,value`). `ExperimentResult.load(directory)` rejects an unknown schema version, a truncated trials file, or a spec that no longer matches its recorded hash. `rerun()` plays the stored spec again; a registry id that no longer exists is reported by name, so Python-defined entries must be registered first.

Ctrl-C stops a run between trials. A cancelled run is `complete == False`.
