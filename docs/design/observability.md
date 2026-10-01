# Observability

Observability is optional. A strategy that publishes nothing still works, and a run with no observer attached pays one null check per decision.

## The model

Observability is a capability, not an interface change. `IStrategy::decide(const Context&)` is untouched.

```text
IDecisionObserver            Strategy/Observability/   pure virtual
└── on_decision(const IState&, const Decision&)

Decision                     data only
├── player, chosen
├── scores                   ActionScore{action, probability, value, has_probability, has_value}
├── extra                    Diagnostics (alias of Metrics: string -> double)
└── tree                     SearchNode{parent, action, visits, value}; flat list, populated by tree searches later
```

- Keys in `extra` are namespaced by strategy: `minimax/nodes`, `minimax/depth_max`.
- `Diagnostics` is `Metrics` (`Core/Metrics.h`), so aggregated diagnostics become experiment metrics with no translation.
- No `std::optional`: scores carry `has_probability` / `has_value` flags.
- Action labels are not stored in a `Decision`; sinks call `IState::action_to_string` when they need text.

## Attaching an observer

```cpp
TraceRecorder trace;
Match match(game, strategies);
match.set_observer(&trace);
match.play();
```

While an observer is attached, `Match` provides it through `Context::provide<IDecisionObserver>()`. If the strategy published nothing during `decide()`, `Match` emits a minimal `Decision` (player and chosen action), so every strategy, including `FirstLegalStrategy` and Python strategies, appears in a trace. The observer is never part of `required_capabilities()`.

The observer must outlive the `Match`, is not thread-safe, and belongs to one `Match` at a time. Exceptions it throws propagate to the caller.

## Making a strategy observable

```cpp
ActionId decide(const Context& context) override
{
    IDecisionObserver* observer = context.get<IDecisionObserver>();
    // ... choose; collect bookkeeping only if observer != nullptr ...
    if (observer != nullptr)
    {
        Decision decision;
        decision.player = player;
        decision.chosen = best_action;
        set_probability(decision, action, probability);
        set_value(decision, action, value);
        add_metric(decision.extra, "mystrategy/nodes", nodes);
        observer->on_decision(state, decision);
    }
    return best_action;
}
```

Rules:

1. **Zero cost when detached.** Check `get<IDecisionObserver>()` once; when null, build no `Decision` and keep hot loops free of extra branches (`MinimaxStrategy` selects a counting or non-counting search once, up front).
2. **Publish after restoring the state.** Strategies that `apply()`/`undo()` must publish once the state is back to the decision position.
3. **Do not leak the observer into sub-contexts.** A strategy that builds its own `Context` (rollouts, nested search) must not provide the observer, so simulated moves never appear as decisions.
4. **Copy, do not retain.** The `Decision` and state are valid only during the call.
5. **Namespace your keys** and keep them from colliding with experiment built-ins (`matches`, `decisions`, `wins/N`, `reward/N`).

`MinimaxStrategy` (depth, nodes, per-root-action values) and `RandomStrategy` (uniform probabilities) are the reference examples.

## Sinks

| Sink | Purpose |
| ---- | ------- |
| `TraceRecorder` | In-memory, one entry per ply |
| `DiagnosticsAggregator` | Folds decisions into `Metrics`: sums plus a per-namespace decision count so means are derivable; keys ending `_max` keep the maximum (`max_metric`), and `merge()` applies the same rule so a peak survives aggregating trials and repeats; keys must be `namespace/name` and may not use the built-in `wins`/`reward` namespaces |
| JSON-lines writer | One object per ply behind a `schema_version` header line, deterministic key order |

Experiments opt in with `RunOptions::collect_diagnostics`; it is not part of `ExperimentSpec`, so the spec hash and `result.yaml` schema are unchanged. A `rerun` must pass the same option.

## Python

`simulate(..., trace=True)` and `Match.trace` return decisions with `Decision.to_dict()`. A Python strategy publishes through `context.observer`, which is `None` when detached.

## Not built

Dashboard transport (Phase 11), fan-out observers, Python-defined observers, populated search trees (MCTS). The seams are `Match::set_observer`, `Match::build_context(..., observer)` and `SearchNode`.
