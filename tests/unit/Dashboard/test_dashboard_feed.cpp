#include "doctest.h"

#include "../Game/DummyGame.h"
#include "unit/MemoryTestSupport.h"

#include "Oasis/Game/TicTacToeGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

Decision make_decision(PlayerId player, ActionId chosen, std::initializer_list<ActionId> actions)
{
    Decision decision;
    decision.player = player;
    decision.chosen = chosen;
    double rank = 0.0;
    for (ActionId action : actions)
    {
        set_probability(decision, action, 0.1 + 0.1 * rank);
        set_value(decision, action, 0.5 - 0.25 * rank);
        rank += 1.0;
    }
    return decision;
}

FeedOptions small_options(int32_t capacity)
{
    FeedOptions options;
    options.capacity = capacity;
    options.max_scores = 4;
    options.max_metrics = 4;
    options.max_keys = 4;
    options.max_tree_nodes = 2;
    return options;
}

} // namespace

TEST_CASE("DashboardFeed copies scores, metrics and labels into a self-contained record")
{
    DummyState state(10);
    DashboardFeed feed(small_options(8));
    Decision decision = make_decision(1, 2, { 1, 2, 3 });
    add_metric(decision.extra, "minimax/nodes", 42);
    feed.on_decision(state, decision);

    REQUIRE(feed.size() == 1);
    RecordView view = feed.view(0);
    REQUIRE(view.valid);
    CHECK(view.record->sequence == 0);
    CHECK(view.record->player == 1);
    CHECK(view.record->chosen == 2);
    CHECK(label_view(view.record->chosen_label) == "take 2");
    CHECK(view.record->chosen_value == doctest::Approx(0.25f));
    CHECK(view.record->chosen_probability == doctest::Approx(0.2f));
    REQUIRE(view.record->score_count == 3);
    CHECK(view.scores[0].action == 1);
    CHECK(label_view(view.scores[2].label) == "take 3");
    CHECK(view.scores[1].has_probability);
    REQUIRE(view.record->metric_count == 1);
    CHECK(feed.key_name(view.metrics[0].key) == "minimax/nodes");
    CHECK(view.metrics[0].value == 42.0);
}

TEST_CASE("DashboardFeed reports an absent chosen score as NaN and gives a sentinel choice no label")
{
    DummyState state(10);
    DashboardFeed feed(small_options(4));
    Decision decision;
    decision.chosen = PENDING_ACTION;
    feed.on_decision(state, decision);

    RecordView view = feed.view(0);
    CHECK(std::isnan(view.record->chosen_value));
    CHECK(std::isnan(view.record->chosen_probability));
    CHECK(label_view(view.record->chosen_label).empty());
    CHECK(view.record->score_count == 0);
}

TEST_CASE("DashboardFeed ring keeps the newest records in order and sequences never repeat")
{
    DummyState state(10);
    DashboardFeed feed(small_options(4));
    for (ActionId i = 1; i <= 10; ++i)
    {
        feed.on_decision(state, make_decision(0, 1, { 1 }));
    }

    CHECK(feed.total_decisions() == 10);
    REQUIRE(feed.size() == 4);
    for (size_t i = 0; i < 4; ++i)
    {
        CHECK(feed.view(i).record->sequence == 6 + i);
    }
    CHECK_FALSE(feed.view(4).valid);
    CHECK_FALSE(feed.find_sequence(5).valid);
    CHECK(feed.find_sequence(6).valid);
    CHECK(feed.find_sequence(9).record->sequence == 9);
    CHECK_FALSE(feed.find_sequence(10).valid);
    CHECK(feed.data()[feed.first_slot()].sequence == 6);
}

TEST_CASE("DashboardFeed truncates to its pools, counts what it dropped and keeps the chosen numbers")
{
    DummyState state(10);
    FeedOptions options = small_options(4);
    options.max_scores = 2;
    options.max_metrics = 1;
    DashboardFeed feed(options);

    Decision decision = make_decision(0, 3, { 1, 2, 3 });
    add_metric(decision.extra, "a/one", 1);
    add_metric(decision.extra, "a/two", 2);
    feed.on_decision(state, decision);

    RecordView view = feed.view(0);
    CHECK(view.record->score_count == 2);
    CHECK(view.record->dropped_scores == 1);
    CHECK(view.record->metric_count == 1);
    CHECK(view.record->dropped_metrics == 1);
    CHECK(view.record->chosen_value == doctest::Approx(0.0f));
    CHECK(feed.drops().scores == 1);
    CHECK(feed.drops().metrics == 1);
}

TEST_CASE("DashboardFeed counts metrics once its key table is full")
{
    DummyState state(10);
    FeedOptions options = small_options(4);
    options.max_keys = 2;
    DashboardFeed feed(options);

    Decision decision;
    add_metric(decision.extra, "a/one", 1);
    add_metric(decision.extra, "b/two", 1);
    add_metric(decision.extra, "c/three", 1);
    feed.on_decision(state, decision);

    CHECK(feed.key_count() == 2);
    CHECK(feed.find_key("c/three") == -1);
    CHECK(feed.view(0).record->dropped_metrics == 1);
}

TEST_CASE("DashboardFeed copies up to max_tree_nodes and none by default")
{
    DummyState state(10);
    Decision decision;
    for (int32_t i = 0; i < 3; ++i)
    {
        SearchNode node;
        node.parent = i - 1;
        node.visits = 10 + i;
        decision.tree.push_back(node);
    }

    DashboardFeed capped(small_options(4));
    capped.on_decision(state, decision);
    RecordView view = capped.view(0);
    REQUIRE(view.record->tree_count == 2);
    CHECK(view.tree[1].visits == 11);
    CHECK(capped.drops().tree_nodes == 1);

    DashboardFeed plain;
    plain.on_decision(state, decision);
    CHECK(plain.view(0).record->tree_count == 0);
}

TEST_CASE("DashboardFeed aggregates per key: sum, count, last, and the maximum for _max keys")
{
    DummyState state(10);
    DashboardFeed feed(small_options(8));
    for (double nodes : { 10.0, 30.0, 20.0 })
    {
        Decision decision;
        add_metric(decision.extra, "minimax/nodes", nodes);
        max_metric(decision.extra, "minimax/depth_max", nodes / 10.0);
        feed.on_decision(state, decision);
    }

    int32_t nodes = feed.find_key("minimax/nodes");
    int32_t depth = feed.find_key("minimax/depth_max");
    REQUIRE(nodes >= 0);
    REQUIRE(depth >= 0);
    const KeyStats& node_stats = feed.key_stats(static_cast<uint16_t>(nodes));
    CHECK(node_stats.sum == 60.0);
    CHECK(node_stats.count == 3);
    CHECK(node_stats.last == 20.0);
    CHECK_FALSE(node_stats.keeps_max);
    const KeyStats& depth_stats = feed.key_stats(static_cast<uint16_t>(depth));
    CHECK(depth_stats.keeps_max);
    CHECK(depth_stats.max == 3.0);
    CHECK(key_namespace("minimax/nodes") == "minimax");
    CHECK(key_namespace("nokey").empty());
}

TEST_CASE("DashboardFeed accepts keys the aggregator would reject instead of throwing")
{
    DummyState state(10);
    DashboardFeed feed(small_options(4));
    Decision decision;
    add_metric(decision.extra, "nokey", 1);
    add_metric(decision.extra, "wins/0", 1);
    CHECK_NOTHROW(feed.on_decision(state, decision));
    CHECK(feed.key_count() == 2);
}

TEST_CASE("DashboardFeed allocates nothing per decision once warm")
{
    if (!MemoryTracker::is_installed())
    {
        return;
    }
    DummyState state(10);
    DashboardFeed feed(small_options(16));
    Decision decision = make_decision(0, 2, { 1, 2, 3 });
    add_metric(decision.extra, "minimax/nodes", 5);
    feed.on_decision(state, decision);

    MemoryStats before = all_allocations();
    for (int32_t i = 0; i < 1000; ++i)
    {
        feed.on_decision(state, decision);
    }
    MemoryStats delta = memory_delta(before, all_allocations());

    CHECK(feed.total_decisions() == 1001);
    CHECK(delta.allocation_count == 0);
}

TEST_CASE("DashboardFeed validates its options")
{
    FeedOptions options;
    options.capacity = 0;
    CHECK_THROWS_AS(DashboardFeed{ options }, Error);
    options = FeedOptions();
    options.max_keys = k_max_feed_keys + 1;
    CHECK_THROWS_AS(DashboardFeed{ options }, Error);
    options = FeedOptions();
    options.max_tree_nodes = -1;
    DashboardFeed feed;
    CHECK_THROWS_AS(feed.reset(options), Error);
}

TEST_CASE("One DashboardFeed serves games with different player and action counts")
{
    DashboardFeed feed(small_options(8));
    DummyState two_player(10);
    feed.on_decision(two_player, make_decision(1, 3, { 1, 2, 3 }));

    FeedOptions wide = small_options(8);
    wide.max_scores = 100;
    feed.reset(wide);
    Decision many;
    many.player = 2;
    many.chosen = 90;
    for (ActionId action = 0; action < 100; ++action)
    {
        set_value(many, action, static_cast<double>(action));
    }
    feed.on_decision(two_player, many);

    RecordView view = feed.view(0);
    CHECK(feed.size() == 1);
    CHECK(view.record->player == 2);
    CHECK(view.record->score_count == 100);
    CHECK(view.record->dropped_scores == 0);
    CHECK(view.record->chosen_value == doctest::Approx(90.0f));
    CHECK(view.scores[99].action == 99);
}

TEST_CASE("begin_match groups decisions per match and numbers them within the match")
{
    DummyState state(10);
    DashboardFeed feed(small_options(8));
    feed.begin_match();
    feed.on_decision(state, make_decision(0, 1, { 1 }));
    feed.on_decision(state, make_decision(1, 1, { 1 }));
    feed.begin_match();
    feed.on_decision(state, make_decision(0, 1, { 1 }));

    CHECK(feed.view(0).record->match_index == 0);
    CHECK(feed.view(1).record->decision_index == 1);
    CHECK(feed.view(2).record->match_index == 1);
    CHECK(feed.view(2).record->decision_index == 0);
}

TEST_CASE("clear_at_next_match empties the ring when the next match begins and only then")
{
    DummyState state(10);
    DashboardFeed feed(small_options(8));
    feed.begin_match();
    feed.on_decision(state, make_decision(0, 1, { 1 }));
    feed.clear_at_next_match();
    feed.on_decision(state, make_decision(0, 1, { 1 }));
    CHECK(feed.size() == 2);

    feed.begin_match();
    CHECK(feed.size() == 0);
    feed.on_decision(state, make_decision(0, 1, { 1 }));
    feed.begin_match();
    CHECK(feed.size() == 1);
    CHECK(feed.view(0).record->sequence == 2);
}

TEST_CASE("A cleared feed never reuses a sequence number")
{
    DummyState state(10);
    DashboardFeed feed(small_options(4));
    feed.on_decision(state, make_decision(0, 1, { 1 }));
    feed.on_decision(state, make_decision(0, 1, { 1 }));
    feed.clear();
    CHECK(feed.size() == 0);
    CHECK(feed.key_count() == 0);
    feed.on_decision(state, make_decision(0, 1, { 1 }));
    CHECK(feed.view(0).record->sequence == 2);
    CHECK_FALSE(feed.find_sequence(1).valid);
}

TEST_CASE("Every registered game and strategy feeds the dashboard one record per decision")
{
    int32_t combinations = 0;
    for (const std::string& game_id : GameRegistry::names())
    {
        UniquePtr<IGame> game = GameRegistry::create(game_id);
        if (game == nullptr)
        {
            continue;
        }
        for (const char* strategy_id : { "random", "first-legal", "minimax" })
        {
            SmallVector<IStrategy*, 2> strategies;
            std::vector<UniquePtr<IStrategy>> owned;
            for (int32_t seat = 0; seat < game->num_players(); ++seat)
            {
                owned.push_back(StrategyRegistry::create(strategy_id));
                strategies.push_back(owned.back().get());
            }
            DashboardFeed feed;
            Match match(*game, std::move(strategies));
            match.set_observer(&feed);
            match.play();

            INFO(game_id << " x " << strategy_id);
            CHECK(feed.total_decisions() == match.history().size());
            for (size_t i = 0; i < feed.size(); ++i)
            {
                CHECK_FALSE(label_view(feed.view(i).record->chosen_label).empty());
            }
            ++combinations;
        }
    }
    CHECK(combinations > 0);
}

TEST_CASE("DashboardFeed records Minimax scores and metrics from a real Match and stops when detached")
{
    oasis::TicTacToeGame game;
    MinimaxStrategy minimax;
    FirstLegalStrategy first_legal;
    SmallVector<IStrategy*, 2> strategies;
    strategies.push_back(&first_legal);
    strategies.push_back(&minimax);

    DashboardFeed feed;
    Match match(game, std::move(strategies));
    match.set_observer(&feed);
    match.apply(match.decide());
    match.apply(match.decide());
    REQUIRE(feed.size() == 2);

    CHECK(feed.view(0).record->score_count == 0);
    RecordView reply = feed.view(1);
    CHECK(reply.record->score_count == 8);
    CHECK_FALSE(std::isnan(reply.record->chosen_value));
    CHECK(feed.find_key("minimax/nodes") >= 0);

    match.set_observer(nullptr);
    match.apply(match.decide());
    CHECK(feed.size() == 2);
}

TEST_CASE("A feed that is never attached stays empty")
{
    DashboardFeed feed;
    oasis::TicTacToeGame game;
    FirstLegalStrategy first_legal;
    SmallVector<IStrategy*, 2> strategies;
    strategies.push_back(&first_legal);
    strategies.push_back(&first_legal);
    Match match(game, std::move(strategies));
    match.play();
    CHECK(feed.size() == 0);
}
