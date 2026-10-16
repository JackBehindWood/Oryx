#include "doctest.h"

#include "ReplayTestSupport.h"

#include "Oasis/Game/TicTacToeGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

// Non-finite numbers are written as null and read back as NaN, so infinity only has to come back non-finite.
bool same_number(double a, double b)
{
    if (std::isnan(a) || std::isinf(a))
    {
        return std::isnan(b);
    }
    return a == b;
}

void check_same_label(const FixedLabel& a, const FixedLabel& b)
{
    CHECK(label_view(a) == label_view(b));
}

void check_same_spaces(const uint32_t* a, uint8_t a_count, const uint32_t* b, uint8_t b_count)
{
    REQUIRE(a_count == b_count);
    for (uint8_t i = 0; i < a_count; ++i)
    {
        CHECK(a[i] == b[i]);
    }
}

void check_same_record(const DashboardFeed& live, const RecordView& a, const DashboardFeed& replay, const RecordView& b)
{
    const DashboardRecord& x = *a.record;
    const DashboardRecord& y = *b.record;
    CHECK(x.sequence == y.sequence);
    CHECK(x.match_index == y.match_index);
    CHECK(x.decision_index == y.decision_index);
    CHECK(x.player == y.player);
    CHECK(x.chosen == y.chosen);
    CHECK(same_number(x.chosen_value, y.chosen_value));
    CHECK(same_number(x.chosen_probability, y.chosen_probability));
    CHECK(x.time_us == y.time_us);
    CHECK(x.dropped_scores == y.dropped_scores);
    CHECK(x.dropped_metrics == y.dropped_metrics);
    CHECK(x.dropped_tree_nodes == y.dropped_tree_nodes);
    check_same_label(x.chosen_label, y.chosen_label);
    check_same_spaces(x.chosen_spaces, x.chosen_space_count, y.chosen_spaces, y.chosen_space_count);

    REQUIRE(x.score_count == y.score_count);
    for (uint32_t i = 0; i < x.score_count; ++i)
    {
        CHECK(a.scores[i].action == b.scores[i].action);
        CHECK(a.scores[i].has_probability == b.scores[i].has_probability);
        CHECK(a.scores[i].has_value == b.scores[i].has_value);
        CHECK(same_number(a.scores[i].probability, b.scores[i].probability));
        CHECK(same_number(a.scores[i].value, b.scores[i].value));
        check_same_label(a.scores[i].label, b.scores[i].label);
        check_same_spaces(a.scores[i].spaces, a.scores[i].space_count, b.scores[i].spaces, b.scores[i].space_count);
    }
    REQUIRE(x.metric_count == y.metric_count);
    for (uint32_t i = 0; i < x.metric_count; ++i)
    {
        CHECK(live.key_name(a.metrics[i].key) == replay.key_name(b.metrics[i].key));
        CHECK(same_number(a.metrics[i].value, b.metrics[i].value));
    }
    REQUIRE(x.tree_count == y.tree_count);
    for (uint32_t i = 0; i < x.tree_count; ++i)
    {
        CHECK(a.tree[i].parent == b.tree[i].parent);
        CHECK(a.tree[i].action == b.tree[i].action);
        CHECK(a.tree[i].visits == b.tree[i].visits);
        CHECK(same_number(a.tree[i].value, b.tree[i].value));
    }
}

FeedOptions wide_live_options()
{
    FeedOptions options;
    options.capacity = 512;
    options.max_scores = k_replay_max_scores;
    options.max_metrics = k_replay_max_metrics;
    options.max_keys = 128;
    options.max_tree_nodes = 64;
    return options;
}

std::string golden_log()
{
    oasis::TicTacToeGame game;
    SharedPtr<const IActionDescriber> describer = make_presenter_describer("tictactoe");
    std::ostringstream out;
    TraceOptions options;
    options.header.game = "tictactoe";
    options.header.players = { "first-legal", "first-legal" };
    options.header.seed = 1;
    options.header.build = "golden";
    options.describer = describer;
    JsonLinesWriter writer(out, options);
    for (int32_t round = 0; round < 2; ++round)
    {
        writer.on_match_start();
        FirstLegalStrategy first;
        FirstLegalStrategy second;
        SmallVector<IStrategy*, 2> strategies;
        strategies.push_back(&first);
        strategies.push_back(&second);
        Match match(game, std::move(strategies));
        match.set_observer(&writer);
        match.play();
    }
    return out.str();
}

std::string read_file(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

std::filesystem::path fixture(const char* name)
{
    return std::filesystem::path(OX_TEST_DATA_DIR) / "replay" / name;
}

} // namespace

TEST_CASE("a replayed log equals what the live feed stored for every registered game and strategy")
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
            INFO(game_id << " x " << strategy_id);
            SharedPtr<const IActionDescriber> describer = make_presenter_describer(game_id);
            DashboardFeed live(wide_live_options());
            live.set_describer(describer);
            std::ostringstream out;
            TraceOptions options;
            options.header.game = game_id;
            options.describer = describer;
            JsonLinesWriter writer(out, options);
            ObserverTee tee;
            tee.add(live);
            tee.add(writer);

            std::vector<UniquePtr<IStrategy>> owned;
            SmallVector<IStrategy*, 2> strategies;
            for (int32_t seat = 0; seat < game->num_players(); ++seat)
            {
                owned.push_back(StrategyRegistry::create(strategy_id));
                strategies.push_back(owned.back().get());
            }
            Match match(*game, std::move(strategies));
            tee.on_match_start();
            match.set_observer(&tee);
            match.play();

            TempLog file(out.str());
            ReplayLog log;
            log.open(file.path());
            REQUIRE(log.record_count() == live.size());
            CHECK(log.malformed_count() == 0);

            DashboardFeed replay;
            DashboardModel model;
            ReplayPlayer player;
            replay_open(player, log, replay, model, ReplayLimits{ 4096, 64 });
            REQUIRE(replay.size() == live.size());
            for (size_t i = 0; i < live.size(); ++i)
            {
                check_same_record(live, live.view(i), replay, replay.view(i));
            }

            REQUIRE(replay.key_count() == live.key_count());
            for (size_t key = 0; key < live.key_count(); ++key)
            {
                int32_t found = replay.find_key(live.key_name(static_cast<uint16_t>(key)));
                REQUIRE(found >= 0);
                const KeyStats& a = live.key_stats(static_cast<uint16_t>(key));
                const KeyStats& b = replay.key_stats(static_cast<uint16_t>(found));
                CHECK(a.count == b.count);
                CHECK(a.sum == b.sum);
                CHECK(a.max == b.max);
                CHECK(a.last == b.last);
                CHECK(a.keeps_max == b.keeps_max);
            }
            ++combinations;
        }
    }
    CHECK(combinations > 0);
}

TEST_CASE("the v1 golden fixture is what the writer produces and the reader understands it")
{
    std::string golden = golden_log();
    if (std::getenv("OX_WRITE_REPLAY_FIXTURE") != nullptr)
    {
        std::ofstream(fixture("v1_sample.jsonl"), std::ios::binary) << golden;
    }
    CHECK(read_file(fixture("v1_sample.jsonl")) == golden);

    ReplayLog log;
    log.open(fixture("v1_sample.jsonl").string());
    CHECK(log.header().game == "tictactoe");
    CHECK(log.header().build == "golden");
    CHECK(log.match_count() == 2);
    CHECK(log.record_count() == 2 * log.match_start(1));
    CHECK(log.malformed_count() == 0);
    CHECK_FALSE(log.has_time());
}

TEST_CASE("the truncated fixture opens with its complete lines and flags the cut")
{
    ReplayLog golden;
    golden.open(fixture("v1_sample.jsonl").string());
    ReplayLog log;
    log.open(fixture("truncated.jsonl").string());
    CHECK(log.record_count() == golden.record_count() - 1);
    CHECK(log.incomplete_tail());
    CHECK(log.malformed_count() == 0);
}

TEST_CASE("the hostile fixture yields only its valid decisions and counts the rest")
{
    ReplayLog log;
    log.open(fixture("hostile.jsonl").string());
    CHECK(log.header().build == "hostile");
    REQUIRE(log.record_count() == 3);
    CHECK(log.malformed_count() == 4);
    CHECK(log.match_count() == 2);
    CHECK(log.key_count() == 2);

    Pools pools;
    REQUIRE(log.read(1, pools.parsed));
    const DashboardRecord& record = pools.parsed.record;
    CHECK(record.match_index == 1);
    CHECK(record.score_count == 1);
    CHECK(label_view(pools.score_pool[0].label) == "\xC3\xA9\xC3\xA9");
    CHECK(std::isnan(pools.score_pool[0].value));
    CHECK(record.metric_count == 2);
    CHECK(record.tree_count == 2);
    CHECK(pools.tree_pool[0].parent == 99);

    REQUIRE(log.read(2, pools.parsed));
    CHECK(pools.parsed.record.player == 1);
    CHECK(pools.parsed.record.chosen == 2);
}
