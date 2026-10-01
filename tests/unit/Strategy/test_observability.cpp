#include "doctest.h"

#include "../Game/DummyGame.h"

#include "Oasis/Game/TicTacToeGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

Decision decision_with(const std::string& key, double value)
{
    Decision decision;
    add_metric(decision.extra, key, value);
    return decision;
}

} // namespace

TEST_CASE("MinimaxStrategy publishes deterministic node counts and root values on an empty Tic-Tac-Toe board")
{
    oasis::TicTacToeState state;
    Context context(state);
    TraceRecorder trace;
    context.provide<IDecisionObserver>(&trace);

    MinimaxStrategy strategy;
    ActionId action = strategy.decide(context);

    REQUIRE(trace.entries().size() == 1);
    const Decision& decision = trace.entries()[0].decision;
    CHECK(decision.chosen == action);
    CHECK(get_metric(decision.extra, "minimax/nodes") == 549945.0);
    CHECK(get_metric(decision.extra, "minimax/depth_max") == 9.0);
    REQUIRE(decision.scores.size() == 9);
    for (const ActionScore& score : decision.scores)
    {
        CHECK(score.has_value);
        CHECK_FALSE(score.has_probability);
        CHECK(score.value == doctest::Approx(0.0));
    }
}

TEST_CASE("MinimaxStrategy picks the same action attached and detached")
{
    oasis::TicTacToeState state;
    for (ActionId move : { 0u, 4u })
    {
        state.apply(move);
    }

    MinimaxStrategy strategy;
    Context detached(state);
    ActionId expected = strategy.decide(detached);

    TraceRecorder trace;
    Context attached(state);
    attached.provide<IDecisionObserver>(&trace);

    CHECK(strategy.decide(attached) == expected);
    CHECK(state.legal_actions().size() == 7);
}

TEST_CASE("RandomStrategy publishes uniform probabilities over the legal actions")
{
    DummyGame game(10);
    UniquePtr<IState> state = game.new_initial_state();
    Context context(*state);
    TraceRecorder trace;
    context.provide<IDecisionObserver>(&trace);

    RandomStrategy strategy(/*seed=*/3);
    ActionId action = strategy.decide(context);

    REQUIRE(trace.entries().size() == 1);
    const Decision& decision = trace.entries()[0].decision;
    CHECK(decision.chosen == action);
    CHECK(decision.scores.size() == state->legal_actions().size());
    double total = 0.0;
    for (const ActionScore& score : decision.scores)
    {
        CHECK(score.has_probability);
        CHECK(score.probability == doctest::Approx(1.0 / static_cast<double>(decision.scores.size())));
        total += score.probability;
    }
    CHECK(total == doctest::Approx(1.0));
}

TEST_CASE("TraceRecorder numbers plies and resolves action labels")
{
    oasis::TicTacToeState state;
    TraceRecorder trace;
    Decision decision;
    decision.chosen = 4;
    set_value(decision, 4, 1.0);
    trace.on_decision(state, decision);
    trace.on_decision(state, decision);

    REQUIRE(trace.entries().size() == 2);
    CHECK(trace.entries()[1].ply == 1);
    CHECK(trace.entries()[0].chosen_label == state.action_to_string(4));
    REQUIRE(trace.entries()[0].score_labels.size() == 1);
    CHECK(trace.entries()[0].score_labels[0] == state.action_to_string(4));
}

TEST_CASE("DiagnosticsAggregator sums keys, keeps maxima and counts decisions per namespace")
{
    oasis::TicTacToeState state;
    DiagnosticsAggregator aggregator;

    Decision first = decision_with("minimax/nodes", 10);
    add_metric(first.extra, "minimax/depth_max", 3);
    Decision second = decision_with("minimax/nodes", 5);
    add_metric(second.extra, "minimax/depth_max", 7);
    aggregator.on_decision(state, first);
    aggregator.on_decision(state, second);
    aggregator.on_decision(state, Decision{});

    CHECK(get_metric(aggregator.metrics(), "minimax/nodes") == 15.0);
    CHECK(get_metric(aggregator.metrics(), "minimax/depth_max") == 7.0);
    CHECK(get_metric(aggregator.metrics(), "minimax/decisions") == 2.0);
}

TEST_CASE("DiagnosticsAggregator rejects un-namespaced and built-in keys")
{
    oasis::TicTacToeState state;
    DiagnosticsAggregator aggregator;

    CHECK_THROWS_AS(aggregator.on_decision(state, decision_with("nodes", 1)), Error);
    CHECK_THROWS_AS(aggregator.on_decision(state, decision_with("wins/0", 1)), Error);
    CHECK_THROWS_AS(aggregator.on_decision(state, decision_with("reward/1", 1)), Error);
    CHECK_THROWS_AS(aggregator.on_decision(state, decision_with("/x", 1)), Error);
}

TEST_CASE("JsonLinesWriter writes a schema header and one line per decision")
{
    oasis::TicTacToeState state;
    std::ostringstream out;
    JsonLinesWriter writer(out);

    Decision decision;
    decision.chosen = 4;
    set_probability(decision, 4, 0.5);
    set_value(decision, 4, 1.5);
    add_metric(decision.extra, "t/n", 2);
    writer.on_decision(state, decision);

    std::vector<std::string> lines;
    std::istringstream in(out.str());
    for (std::string line; std::getline(in, line);)
    {
        lines.push_back(line);
    }
    REQUIRE(lines.size() == 2);
    CHECK(lines[0] == "{\"schema_version\":1}");
    CHECK(lines[1].find("\"ply\":0") != std::string::npos);
    CHECK(lines[1].find("\"probability\":0.5") != std::string::npos);
    CHECK(lines[1].find("\"value\":1.5") != std::string::npos);
    CHECK(lines[1].find("\"extra\":{\"t/n\":2}") != std::string::npos);
}

TEST_CASE("run_trial adds strategy diagnostics only when collect_diagnostics is set")
{
    ExperimentSpec spec;
    spec.name = "diagnostics";
    StrategySpec minimax{ "minimax", {} };
    StrategySpec random{ "random", {} };
    spec.matchups = { Matchup{ "", "tictactoe", {}, { random, minimax } } };
    spec.matches_per_trial = 2;
    spec.repeats = 1;
    spec.master_seed = 5;

    TrialResult plain = run_trial(spec, 0, 0);
    CHECK_FALSE(has_metric(plain.metrics, "minimax/nodes"));

    RunOptions options;
    options.collect_diagnostics = true;
    TrialResult observed = run_trial(spec, 0, 0, options);
    CHECK(get_metric(observed.metrics, "minimax/nodes") > 0.0);
    CHECK(get_metric(observed.metrics, "random/decisions") > 0.0);
    CHECK(get_metric(observed.metrics, "wins/0") == get_metric(plain.metrics, "wins/0"));
}

namespace
{

Params seeded(const std::string& id)
{
    Params params;
    const EntryInfo* info = StrategyRegistry::info(id);
    if (info != nullptr)
    {
        for (const ParamSpec& spec : info->schema)
        {
            if (spec.name == "seed")
            {
                params["seed"] = static_cast<int64_t>(7);
            }
        }
    }
    return params;
}

struct Played
{
    std::vector<ActionId> actions;
    std::vector<double> rewards;
};

Played play_game(const IGame& game, const std::vector<std::string>& ids, IDecisionObserver* observer)
{
    std::vector<UniquePtr<IStrategy>> owned;
    SmallVector<IStrategy*, 2> strategies;
    for (const std::string& id : ids)
    {
        owned.push_back(StrategyRegistry::create(id, seeded(id)));
        strategies.push_back(owned.back().get());
    }

    Match match(game, std::move(strategies));
    match.set_observer(observer);
    Outcome outcome = match.play();

    Played played;
    for (ActionId action : match.history().actions())
    {
        played.actions.push_back(action);
    }
    for (size_t player = 0; player < outcome.rewards.player_count(); ++player)
    {
        played.rewards.push_back(outcome.rewards[static_cast<PlayerId>(player)]);
    }
    return played;
}

} // namespace

TEST_CASE("Every registered game and strategy plays identically with an observer attached or detached")
{
    int32_t combinations = 0;
    for (const std::string& game_id : GameRegistry::names())
    {
        UniquePtr<IGame> game = GameRegistry::create(game_id);
        if (game == nullptr)
        {
            continue;
        }

        for (const std::string& strategy_id : StrategyRegistry::names())
        {
            size_t slash = strategy_id.find('/');
            if (slash != std::string::npos && strategy_id.substr(0, slash) != game_id)
            {
                continue;
            }

            UniquePtr<IStrategy> probe = StrategyRegistry::create(strategy_id, seeded(strategy_id));
            UniquePtr<IState> state = game->new_initial_state();
            Context context = Match::build_context(*game, *state);
            if (probe == nullptr || !Match::missing_capabilities(*probe, context).empty())
            {
                continue;
            }

            std::vector<std::string> seats(static_cast<size_t>(game->num_players()), strategy_id);
            TraceRecorder trace;
            Played detached = play_game(*game, seats, nullptr);
            Played attached = play_game(*game, seats, &trace);

            INFO(game_id << " x " << strategy_id);
            CHECK(attached.actions == detached.actions);
            CHECK(attached.rewards == detached.rewards);
            CHECK(trace.entries().size() == attached.actions.size());
            ++combinations;
        }
    }
    CHECK(combinations > 0);
}

TEST_CASE("Merging metrics sums counters but keeps the maximum of _max keys")
{
    Metrics total;
    Metrics first;
    add_metric(first, "minimax/nodes", 10);
    add_metric(first, "minimax/depth_max", 3);
    Metrics second;
    add_metric(second, "minimax/nodes", 5);
    add_metric(second, "minimax/depth_max", 7);
    merge(total, first);
    merge(total, second);
    merge(total, first);
    CHECK(get_metric(total, "minimax/nodes") == 25.0);
    CHECK(get_metric(total, "minimax/depth_max") == 7.0);
    CHECK(is_max_metric("a/b_max"));
    CHECK_FALSE(is_max_metric("_max"));
    CHECK_FALSE(is_max_metric("a/max"));
}

TEST_CASE("JsonLinesWriter escapes control characters and writes non-finite numbers as null")
{
    oasis::TicTacToeState state;
    std::ostringstream out;
    JsonLinesWriter writer(out);
    Decision decision;
    decision.player = 0;
    decision.chosen = 4;
    add_metric(decision.extra, "x/\"quoted\"\tkey", std::numeric_limits<double>::quiet_NaN());
    add_metric(decision.extra, "x/inf", std::numeric_limits<double>::infinity());
    writer.on_decision(state, decision);
    std::string text = out.str();
    CHECK(text.find("\\\"quoted\\\"\\tkey\":null") != std::string::npos);
    CHECK(text.find("\"x/inf\":null") != std::string::npos);
}

TEST_CASE("Aggregating repeats keeps a diagnostic peak instead of summing it")
{
    ExperimentSpec spec;
    spec.name = "peak";
    spec.matchups = { Matchup{ "", "tictactoe", {}, { StrategySpec{ "random", {} }, StrategySpec{ "minimax", {} } } } };
    spec.matches_per_trial = 1;
    spec.repeats = 3;
    RunOptions options;
    options.collect_diagnostics = true;
    ExperimentResult result = run_experiment(spec, options);
    double largest = 0.0;
    for (const TrialResult& trial : result.trials)
    {
        largest = std::max(largest, get_metric(trial.metrics, "minimax/depth_max"));
    }
    CHECK(largest > 0.0);
    CHECK(get_metric(aggregate(result, matchup_key(spec.matchups[0])), "minimax/depth_max") == largest);
}
