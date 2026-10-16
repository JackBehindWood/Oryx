# Observability

Observability is optional. A strategy that publishes nothing still works, and a run with no observer attached pays one null check per decision.

## The model

Observability is a capability, not an interface change. `IStrategy::decide(const Context&)` is untouched.

```text
IDecisionObserver            Strategy/Observability/   pure virtual
└── on_decision(const IState&, const Decision&)
└── on_match_start()          default no-op; marks a new match on the decision stream (Phase 11)

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

6. **What the dashboard shows.** Scores are optional: the Trace view needs only `player` and `chosen`, so a strategy that publishes nothing still appears there. Metric keys are `namespace/name` doubles; the Stats view groups by the text before the first `/` and shows last, mean, max and count per key, and a key ending `_max` keeps its maximum. No dashboard code is needed per strategy.

`MinimaxStrategy` (depth, nodes, per-root-action values) and `RandomStrategy` (uniform probabilities) are the reference examples.

## Sinks

| Sink | Purpose |
| ---- | ------- |
| `TraceRecorder` | In-memory, one entry per ply |
| `DiagnosticsAggregator` | Folds decisions into `Metrics`: sums plus a per-namespace decision count so means are derivable; keys ending `_max` keep the maximum (`max_metric`), and `merge()` applies the same rule so a peak survives aggregating trials and repeats; keys must be `namespace/name` and may not use the built-in `wins`/`reward` namespaces |
| `JsonLinesWriter` | The trace log (`Strategy/Observability/TraceSchema.h`): a `kind:"header"` line (schema version, game, params, players, seed, build) then one `kind:"decision"` line per decision and a `kind:"match_end"` line (`match`, `rewards`) when the observer's `on_match_end(outcome)` fires (readers skip it until replay uses it) with `match`, per-match `ply`, `player`, `chosen`, labels and board `spaces` (through an optional `IActionDescriber`), scores, `extra`, `tree`, and optional `t_us`. Deterministic key order, doubles at `max_digits10`, non-finite as `null`. Additive fields and kinds never bump `schema_version`; readers skip what they do not know and reject a newer version. Oasis `--record FILE` writes one (a game switch starts `FILE-2`) |
| `ReplayLog` | The trace reader (`Dashboard/Feed/ReplayLog.h`, headless): one streaming pass indexes decision lines (8 B each) and builds the whole-log key table and stats; `read(index, ParsedRecord&)` re-reads one line into caller-owned pools without allocating. Only a missing or invalid header, or a newer `schema_version`, throws (at `open`); bad lines (malformed, over 1 MiB, nested past 32 levels) are skipped and counted, CRLF and a BOM are tolerated, an unterminated last line waits for `refresh()`, and `dashboard.replay_max_records` caps the index. `DashboardFeed::load_window` shows a window of it so `sequence == log index`; `ReplayPlayer` drives the playhead |
| `ObserverTee` | Fan-out to several observers (feed + recorder) |
| `DashboardFeed` | Bounded, allocation-free ring for the live dashboard (`Dashboard/Feed`): fixed pools, labels and board spaces captured at `on_decision` through an optional `IActionDescriber`, per-key aggregates, lenient keys; see [Dashboard](dashboard.md) |

Experiments opt in with `RunOptions::collect_diagnostics`; it is not part of `ExperimentSpec`, so the spec hash and `result.yaml` schema are unchanged. A `rerun` must pass the same option.

## Python

`simulate(..., trace=True)` and `Match.trace` return decisions with `Decision.to_dict()`. A Python strategy publishes through `context.observer`, which is `None` when detached.

## Not built

Dashboard transport (Phase 11), Python-defined observers, populated search trees (MCTS). The seams are `Match::set_observer`, `Match::build_context(..., observer)` and `SearchNode`.

## The in-app console (Step 9)

Two headless pieces feed the Oasis console drawer and are usable on their own:

- **Log capture** (`Core/LogCapture.h`): when built (`log_capture` forge option, on by default, `--without log_capture` off) both loggers also write into a fixed ring of 2048 lines (level, logger, sequence, 240 bytes of text) behind one mutex, so simulation threads may log while the UI reads. `log_capture::snapshot` copies the held lines oldest first; `dropped()` counts what the ring overwrote. Lines logged before `Log::init` are not captured.
- **Console commands** (`Debug/ConsoleCommands.h`): `OX_CONSOLE_COMMAND(id, "usage", "help") { ... }` registers one command (`OX_REGISTER_CONSOLE_COMMAND` for an existing function, `OX_CONSOLE_COMMAND_ALIAS` for a second name); `console::run` runs a line and prints an `Error` instead of throwing. Without `OX_ENABLE_CONSOLE_COMMANDS` the macros register nothing. Built in: `help`, `clear`, `log.level <core|app> <level>`, `stats.memory`; workbench commands join as `<workbench>.<command>`.

## Cost of attaching a feed (Step 9)

Measured with a seeded Tic-Tac-Toe match (`test_dashboard_feed.cpp`): with no observer a match allocates nothing; with a `DashboardFeed` attached it allocates about one block per decision for `random` and about eight for `minimax`, all of them the `Decision`'s own containers (`scores`, `extra`, `tree`) that a strategy fills only when an observer is attached. The feed copies them into its ring without allocating. The `Decision::extra` map is therefore not hot (a decision of `minimax` searches thousands of nodes), and no fixed-capacity slot was added.

**Diagnostic keys** are `namespace/name`. The namespace is the emitting strategy's id (`random/legal_actions`, `minimax/nodes`) or one of the shared families a new strategy should prefer when its numbers mean the same across strategies: `search/` (nodes, depth, simulations), `eval/` (evaluations, value estimates) and `policy/` (entropy, priors). `wins` and `reward` are reserved for the experiment runner. The registry-driven test "Every registered game and strategy plays identically with an observer attached or detached" checks every key it sees.
