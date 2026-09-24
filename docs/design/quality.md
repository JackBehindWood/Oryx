# Quality and Performance

## Testing

Testing should exist at several levels.

### Unit tests

For:

* Game rules
* State transitions
* Action validation
* Strategy components
* Utility algorithms

### Integration tests

For:

* Game + strategy execution
* Simulation
* Engine workflows
* Python bindings

Python bindings are split by what they need. `tests/unit/Python/` and `tests/integration/`
(doctest, in the `Tests` binary) cover the embedded runtime's lifecycle, the GIL, C++-computed
expectations, and anything that needs a C++-native game (Oasis isn't linked into the standalone
research-host extension, so only `tests/integration/test_research_host.cpp` and a same-named
scripted game can stand in for one there). `tests/python/` (pytest, `forge test python`) covers
pure Python-visible behaviour of `import oryx` against the research host, using Oasis's own
scripted `nim`/`monte-carlo` example as its test game and strategy.

### Regression tests

For:

* Known algorithm behaviour
* Deterministic scenarios
* Previously discovered bugs

### Statistical tests

Where appropriate for stochastic algorithms.

Tests should avoid assuming that a stochastic result must equal one exact outcome unless the seed and execution model guarantee it.

## Performance

The project should follow:

> Measure first, optimise second.

Likely performance-sensitive areas include:

* State copying
* Action generation
* Simulation loops
* Search trees
* Monte Carlo rollouts
* Memory allocation
* Python/C++ boundary crossings
* Parallel simulation

Performance abstractions should be introduced based on profiling and benchmark evidence.

### Allocation audit (2026-09)

A first pass audited where `Oryx`/`Oasis` actually allocate, using the new
`oryx::Instrumentation`/`ScopeTimer` utility (`Oryx/Debug/Instrumentation.h`)
and `tests/benchmark/bench_minimax_allocation.cpp`. Classification:

* **Hot** (scales with search-tree nodes, not just matches/turns):
  `IState::legal_actions()` — a fresh heap `std::vector<ActionId>` per call,
  invoked once per node by `MinimaxStrategy`'s unmemoized recursive search
  (`MinimaxStrategy.cpp`); `Rewards<double>` return-by-value in
  `MinimaxStrategy::evaluate()` (also a heap vector, via `Outcome.h`).
* **Warm** (once per turn or per match): `Context`'s
  `std::unordered_map<std::type_index, void*>` and
  `Match::missing_capabilities()` (once per `decide()`, i.e. per turn, not
  per node); `IGame::new_initial_state()` and `ActionHistory`'s two vectors
  (once per `Match`, in `BatchRunner::run()`'s per-match loop).
* **Cold** (once per process): `Registry<T>` entries, `IGame`/`IStrategy`
  construction. `oryx::UniquePtr`/`SharedPtr` (`Base.h`) are thin aliases
  used only for these one-shot constructions — not implicated here.

Benchmark evidence (DummyGame, pile size 20, full exhaustive Minimax search,
Debug build, Apple Silicon): 144,664 `legal_actions()` calls took ~24% of
total `decide()` wall time even with instrumentation overhead included. An
isolated micro-benchmark comparing a heap `std::vector` holding a
`legal_actions()`-shaped (<=3 element) result against an equivalent
fixed-size stack array found ~717 ns/call for the heap vector vs. ~1.8
ns/call for the array — roughly 390x, purely from heap traffic on a
result that's always small.

**Decision: narrow fix identified, not yet implemented.** The evidence
supports a small-buffer-optimized (SBO) vector-like return type for
`IState::legal_actions()` specifically — sized inline for the branching
factors seen so far (<=9, e.g. Tic-Tac-Toe/`DummyGame`) — rather than a
general-purpose pool/arena allocator (no evidence yet that `Match`/
`BatchRunner`-level allocations are hot enough to justify one) or touching
`UniquePtr`/`SharedPtr` (not implicated by this audit). Implementing the SBO
container is deliberately deferred to a separate change, gated on this
finding rather than bundled with the audit itself ([Design Review Principle](principles.md#design-review-principle)).

**Outcome (2026-09, implemented).** `SmallVector<T, N>` (`Oryx/Containers/SmallVector.h`)
replaced `std::vector` as `IState::legal_actions()`'s return type
(`ActionList = SmallVector<ActionId, 9>`) and `Rewards<T>`'s internal storage.
Re-running `bench_minimax_allocation.cpp`: `legal_actions()`'s share of
`MinimaxStrategy::decide()` wall time dropped from ~24% to ~6.2% (144,664
calls, same `DummyGame` workload); the isolated micro-benchmark's ~707ns/call
heap-vector cost fell to ~31.6ns/call for `ActionList` (~22x — vs. ~1.8ns/call
for a raw fixed array; some per-push overhead remains, but the heap
allocation is gone).

Beyond the audited fix, at the user's explicit direction and without separate
benchmarking ([Documentation as a Design Tool](platform.md#documentation-as-a-design-tool)/[Design Review Principle](principles.md#design-review-principle) — recorded here rather than silently decided in code),
the same container was also applied to `Match`/`BatchRunner`/`SimulationLayer`'s
strategy-holding vectors, `BatchResult::wins` (all sized by `num_players()`,
inline capacity 2), and `IActionFeatures::decode()` (inline capacity 2).
`Registry<T>` separately moved from `std::unordered_map` to a new
open-addressing `FlatHashMap<Key, Value, N>` (`Oryx/Containers/FlatHashMap.h`, over
a new `Pair<K,V>` type) — also unaudited, since `Registry<T>` is Cold. Both
containers spill to the heap past their inline capacity, so neither caps
entry/element counts. Left out of scope: `ActionHistory`, the capability
vectors (`missing_capabilities()`/`required_capabilities()`), `Registry::names()`,
game-specific strategy-lookup redesign, and a runtime-configurable-capacity
sibling to `SmallVector` (no current caller needs one).

### Python adapter cost (2026-09)

`tests/benchmark/bench_python_adapters.cpp` (built when Python is on; run `Tests --test-suite=benchmark --test-case="Benchmark: Python*,Benchmark: end-to-end*"`) times the C++ adapter around a Python `NimState` and Python strategies. Apple M3, CPython 3.11, median of runs; ns per call from C++ into Python:

| Call | first implementation, Release | current, Release | current, Debug |
| ---- | ----------------------------- | ---------------- | -------------- |
| `legal_actions` | 298 | 153 | 280 |
| `current_player` / `is_terminal` | 85 / 84 | 23 / 24 | 63 / 53 |
| `outcome` | 281 | 99 | 540 |
| `apply` / `undo` | 100 / 100 | 41 / 40 | 84 / 85 |
| `decide` (Python strategy, first legal action) | 1232 | 504 | 6960 |
| Python Monte Carlo on Python Nim, decisions/s | 730 | 1290 | 194 |

The speed-up has two sources. The backend restructure removed per-call string building, the generic argument and result casting and the bound-method allocation: each scripted class gets a method table once (compile-time method sets, `PyObject_Vectorcall`) and results are converted through the C API. Then one measured cache: a scripted state remembers its last `legal_actions()` until its own `apply`/`undo`, because the legality check of `apply()` asked the script for the same list a moment after the strategy had (+17 to +18 % on Python Monte Carlo and on the end-to-end Python Nim batch). Measured and **not** kept because they are within run-to-run noise (1 to 4 %): skipping `PyGILState_Ensure` when the GIL is already held, and link-time optimisation of Release. A reusable `Context` was not tried, because it would make a stashed handle from an earlier `decide()` valid again. Debug is 2 to 5 times slower than Release for this path (pybind11 checks and unoptimised adapters), so changes are judged on Release. A Python-defined game or state stays the slow path (D22): C++ games with Python strategies and all-C++ batches are the fast ones.

### Allocator module (2026-09, step 13)

`decide` has been through three states, all Release, measured on the same machine:

| State | `decide` ns |
| ----- | ----------- |
| After the backend restructure and the legal-actions cache | 510 |
| The allocation census sizes every free through `malloc_size` (the fix for the Release `import oryx` crash) | 587 |
| Oryx objects from the size-class pool, freed with their exact size ([Memory Allocator](memory-allocator.md)) | 504 |

The `decide` path allocates three small objects per call (`ScriptLease`, `PyContext`, `PyState`). The census cost about 77 ns of it (confirmed by stubbing the size lookup). The pool removes the lookup entirely, because every Oryx pointer frees through the allocator recorded in its block, with the size it asked for.

Each switch was gated by an **interleaved** A/B: two binaries run alternately, 12 to 15 times each, medians compared. Samples taken minutes apart drifted by 1 to 2 %, and a first, non-interleaved comparison showed a regression that disappeared once interleaved. Results against the previous step:

| Step | `decide` | end-to-end Nim batch |
| ---- | -------- | -------------------- |
| Own `UniquePtr`/`SharedPtr`, still on the heap | 593 → 584 | within noise |
| The pool as the default, every memory report on its counts | 576 → 515 | −0.4 % pool share, inside the ±2 % spread |
| Containers on the allocator | 515 → 504 | within noise |

One routing detail was measurable: choosing the size class with a loop and a `%` per request cost about 1 % end to end, so it became two compile-time lookup tables. Memory reports now count Oryx objects exactly (the pool's counts), including in the Python extension (`oryx.benchmark(memory=True)`); the census remains the whole-program number in executables.

## Parallelism

Large-scale simulation is an important potential workload.

However, concurrency should not be built into every abstraction prematurely.

The likely design direction is to make simulations independently executable where possible:

```text
Experiment
    │
    ├── Simulation 1
    ├── Simulation 2
    ├── Simulation 3
    ├── ...
    └── Simulation N
```

This can provide a natural basis for parallel execution.

Phase 5's batch runner takes the first concrete step here — but stays
single-threaded. Parallel execution is explicitly scoped out of Phase 5
rather than left ambient: batching this phase exists to prove the `Match`/
runner API shape (one game + two strategies → `Outcome`, aggregated over N
runs), not to deliver throughput. The exact threading/executor model
remains open for a later phase.
