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
        CHECK(score.has_probability);
        CHECK(score.probability == doctest::Approx(1.0 / 9.0));
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
    CHECK(lines[0].find("\"kind\":\"header\",\"schema_version\":1") != std::string::npos);
    CHECK(lines[1].find("\"kind\":\"decision\",\"match\":0,\"ply\":0") != std::string::npos);
    CHECK(lines[1].find("\"probability\":0.5") != std::string::npos);
    CHECK(lines[1].find("\"value\":1.5") != std::string::npos);
    CHECK(lines[1].find("\"extra\":{\"t/n\":2}") != std::string::npos);
}

namespace
{

class FixedSpaces final : public IActionDescriber
{
public:
    bool describe(const IState&, ActionId action, ActionDescription& out) const override
    {
        set_label(out.label, "cell" + std::to_string(action));
        out.spaces[0] = static_cast<uint32_t>(action);
        out.space_count = 1;
        return true;
    }
};

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);)
    {
        lines.push_back(line);
    }
    return lines;
}

} // namespace

TEST_CASE("JsonLinesWriter header names the game, params, players and seed")
{
    std::ostringstream out;
    TraceOptions options;
    options.header.game = "tictactoe";
    options.header.params = { { "size", int64_t(3) }, { "misere", false } };
    options.header.players = { "me", "random" };
    options.header.seed = 42;
    JsonLinesWriter writer(out, options);
    CHECK(out.str() == "{\"kind\":\"header\",\"schema_version\":1,\"game\":\"tictactoe\",\"params\":{\"misere\":false,\"size\":3},\"players\":[\"me\",\"random\"],\"seed\":42,\"build\":\"\"}\n");
}

TEST_CASE("JsonLinesWriter numbers plies per match and writes spaces from the describer")
{
    oasis::TicTacToeState state;
    std::ostringstream out;
    TraceOptions options;
    options.describer = create_shared<FixedSpaces>();
    JsonLinesWriter writer(out, options);
    Decision decision;
    decision.chosen = 4;
    set_value(decision, 4, 1.0);
    writer.on_match_start();
    writer.on_decision(state, decision);
    writer.on_decision(state, decision);
    writer.on_match_start();
    writer.on_decision(state, decision);

    std::vector<std::string> lines = split_lines(out.str());
    REQUIRE(lines.size() == 4);
    CHECK(lines[1].find("\"match\":0,\"ply\":0") != std::string::npos);
    CHECK(lines[2].find("\"match\":0,\"ply\":1") != std::string::npos);
    CHECK(lines[3].find("\"match\":1,\"ply\":0") != std::string::npos);
    CHECK(lines[1].find("\"chosen_label\":\"cell4\",\"chosen_spaces\":[4]") != std::string::npos);
    CHECK(lines[1].find("\"label\":\"cell4\",\"spaces\":[4],\"value\":1") != std::string::npos);
    CHECK(lines[1].find("t_us") == std::string::npos);
}

TEST_CASE("JsonLinesWriter adds t_us only when asked")
{
    oasis::TicTacToeState state;
    std::ostringstream out;
    TraceOptions options;
    options.record_time = true;
    JsonLinesWriter writer(out, options);
    Decision decision;
    decision.chosen = 4;
    writer.on_decision(state, decision);
    CHECK(split_lines(out.str())[1].find("\"t_us\":") != std::string::npos);
}

TEST_CASE("ObserverTee forwards decisions and match starts to every target in order")
{
    struct Counter final : IDecisionObserver
    {
        int32_t decisions = 0;
        int32_t starts = 0;
        void on_decision(const IState&, const Decision&) override { ++decisions; }
        void on_match_start() override { ++starts; }
    };
    oasis::TicTacToeState state;
    Counter first;
    Counter second;
    ObserverTee tee;
    tee.add(first);
    tee.add(second);
    tee.on_match_start();
    tee.on_decision(state, Decision{});
    CHECK(first.decisions == 1);
    CHECK(second.starts == 1);
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
            const std::string own = strategy_id.substr(slash == std::string::npos ? 0 : slash + 1);
            for (const TraceEntry& entry : trace.entries())
            {
                for (const auto& [key, value] : entry.decision.extra.values)
                {
                    const std::string family = key.substr(0, key.find('/'));
                    INFO("diagnostic key '" << key << "'");
                    CHECK(key.find('/') != std::string::npos);
                    CHECK((family == own || family == "search" || family == "eval" || family == "policy"));
                }
            }
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

TEST_CASE("MinimaxStrategy publishes its top plies as a tree and a policy over the optimal moves")
{
    oasis::TicTacToeState state;
    Context context(state);
    TraceRecorder trace;
    context.provide<IDecisionObserver>(&trace);

    MinimaxStrategy strategy;
    strategy.decide(context);

    const Decision& decision = trace.entries()[0].decision;
    REQUIRE(!decision.tree.empty());
    CHECK(decision.tree[0].parent == -1);
    CHECK(decision.tree[0].visits == 549945);
    CHECK(decision.tree.size() == 1 + 9 + 9 * 8);
    for (size_t i = 1; i < decision.tree.size(); ++i)
    {
        CHECK(decision.tree[i].parent >= 0);
        CHECK(static_cast<size_t>(decision.tree[i].parent) < i);
    }
    double total = 0.0;
    for (const ActionScore& score : decision.scores)
    {
        CHECK(score.has_probability);
        total += score.probability;
    }
    CHECK(total == doctest::Approx(1.0));
}

namespace
{

struct EndCounter : IDecisionObserver
{
    void on_decision(const IState&, const Decision&) override { ++decisions; }
    void on_match_end(const Outcome& outcome) override
    {
        ++ends;
        players = outcome.rewards.player_count();
    }
    int32_t decisions = 0;
    int32_t ends = 0;
    size_t players = 0;
};

} // namespace

TEST_CASE("Match::play notifies on_match_end once, through an ObserverTee, after the last decision")
{
    oasis::TicTacToeGame game;
    FirstLegalStrategy first;
    FirstLegalStrategy second;
    SmallVector<IStrategy*, 2> strategies;
    strategies.push_back(&first);
    strategies.push_back(&second);
    EndCounter counter;
    std::ostringstream out;
    JsonLinesWriter writer(out);
    ObserverTee tee;
    tee.add(counter);
    tee.add(writer);
    Match match(game, std::move(strategies));
    match.set_observer(&tee);
    match.play();

    CHECK(counter.ends == 1);
    CHECK(counter.players == 2);
    CHECK(counter.decisions > 0);
    std::string text = out.str();
    size_t end = text.rfind("{\"kind\":\"match_end\",\"match\":0,\"rewards\":[");
    REQUIRE(end != std::string::npos);
    CHECK(text.find('\n', end) == text.size() - 1);
    CHECK(text.rfind("\"kind\":\"decision\"") < end);
}
