#include "doctest.h"

#include "../Game/DummyGame.h"
#include "ReplayTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

constexpr ReplayLimits k_small_window{ 8, 4 };

struct Rig
{
    explicit Rig(const std::string& content, ReplayLimits limits = k_small_window)
        : file(content)
    {
        log.open(file.path());
        replay_open(player, log, feed, model, limits);
    }

    TempLog file;
    ReplayLog log;
    DashboardFeed feed;
    DashboardModel model;
    ReplayPlayer player;
};

void check_selection_follows(const Rig& rig)
{
    CHECK(rig.model.selected_sequence == rig.player.playhead);
    CHECK_FALSE(rig.model.follow_latest);
    RecordView view = selected_view(rig.model);
    REQUIRE(view.valid);
    CHECK(view.record->sequence == rig.player.playhead);
}

} // namespace

TEST_CASE("replay_feed_options sizes the pools to the log and caps them")
{
    TempLog file(join_lines({ header_line(), full_decision(0, 0) }));
    ReplayLog log;
    log.open(file.path());
    FeedOptions options = replay_feed_options(log, ReplayLimits{ 4096, 1 });
    CHECK(options.capacity == 1);
    CHECK(options.max_scores == 2);
    CHECK(options.max_metrics == 2);
    CHECK(options.max_tree_nodes == 1);
    CHECK(options.max_keys == 2);

    TempLog empty(std::string(header_line()) + "\n");
    ReplayLog nothing;
    nothing.open(empty.path());
    FeedOptions floor = replay_feed_options(nothing, ReplayLimits{ 4096, 0 });
    CHECK(floor.capacity == 1);
    CHECK(floor.max_scores == 1);
    CHECK(floor.max_tree_nodes == 0);
}

TEST_CASE("a window feed holds sequence == log index and finds only resident sequences")
{
    Rig rig(synthetic_log(3, 10));
    CHECK(rig.feed.replay_sourced());
    CHECK(rig.feed.capacity() == 8);
    CHECK(rig.feed.size() == 8);
    CHECK(rig.feed.oldest_sequence() == 0);

    replay_seek(rig.player, 20);
    uint64_t oldest = rig.feed.oldest_sequence();
    uint64_t end = rig.feed.total_decisions();
    REQUIRE(end - oldest == 8);
    CHECK(oldest <= 20);
    CHECK(end > 20);
    for (uint64_t sequence = oldest; sequence < end; ++sequence)
    {
        RecordView view = rig.feed.find_sequence(sequence);
        REQUIRE(view.valid);
        CHECK(view.record->sequence == sequence);
        CHECK(view.record->match_index == sequence / 10);
        CHECK(view.record->decision_index == sequence % 10);
    }
    CHECK(rig.feed.find_sequence(oldest).valid);
    CHECK(rig.feed.find_sequence(end - 1).valid);
    CHECK_FALSE(rig.feed.find_sequence(oldest - 1).valid);
    CHECK_FALSE(rig.feed.find_sequence(end).valid);
}

TEST_CASE("a log that fits the window is fully resident and never refills")
{
    Rig rig(synthetic_log(1, 5), ReplayLimits{ 4096, 4 });
    CHECK(rig.feed.capacity() == 5);
    CHECK(rig.feed.size() == 5);
    replay_to_end(rig.player);
    CHECK(rig.feed.oldest_sequence() == 0);
    CHECK(rig.feed.total_decisions() == 5);
    CHECK(rig.feed.find_sequence(4).valid);
}

TEST_CASE("a replay-sourced feed ignores live decisions until it is reset")
{
    Rig rig(synthetic_log(1, 5));
    DummyState state(5);
    Decision decision;
    decision.chosen = 1;
    rig.feed.on_decision(state, decision);
    rig.feed.begin_match();
    rig.feed.clear_at_next_match();
    rig.feed.begin_match();
    CHECK(rig.feed.total_decisions() == 5);
    CHECK(rig.feed.size() == 5);

    rig.feed.reset(FeedOptions());
    CHECK_FALSE(rig.feed.replay_sourced());
    rig.feed.on_decision(state, decision);
    CHECK(rig.feed.size() == 1);
}

TEST_CASE("the window adopts the whole log's keys and stats, not just the resident ones")
{
    std::string text = std::string(header_line()) + "\n";
    for (int32_t i = 0; i < 20; ++i)
    {
        text += full_decision(0, i) + "\n";
    }
    Rig rig(text);
    REQUIRE(rig.feed.key_count() == 2);
    CHECK(rig.feed.key_name(0) == "minimax/nodes");
    CHECK(rig.feed.key_stats(0).count == 20);
    CHECK(rig.feed.key_stats(0).sum == 200);
    CHECK(rig.feed.find_key("depth_max") == 1);
}

TEST_CASE("a warm window refill and slide allocate nothing")
{
    if (!MemoryTracker::is_installed())
    {
        return;
    }
    std::string text = std::string(header_line()) + "\n";
    for (int32_t i = 0; i < 200; ++i)
    {
        text += full_decision(i / 50, i % 50) + "\n";
    }
    Rig rig(text);
    replay_seek(rig.player, 150);
    replay_seek(rig.player, 3);

    MemoryStats before = all_allocations();
    for (uint64_t target : { uint64_t{ 190 }, uint64_t{ 20 }, uint64_t{ 21 }, uint64_t{ 199 }, uint64_t{ 0 }, uint64_t{ 100 } })
    {
        replay_seek(rig.player, target);
    }
    replay_play(rig.player);
    for (int32_t i = 0; i < 100; ++i)
    {
        replay_tick(rig.player, 0.5);
    }
    CHECK(memory_delta(before, all_allocations()).allocation_count == 0);
}

TEST_CASE("replay_step, seek and the jumps clamp to the log and drive the selection")
{
    Rig rig(synthetic_log(3, 10));
    ReplayPlayer& player = rig.player;
    check_selection_follows(rig);

    replay_step(player, -5);
    CHECK(player.playhead == 0);
    replay_step(player, 1);
    CHECK(player.playhead == 1);
    replay_step(player, 1000000);
    CHECK(player.playhead == 29);
    replay_step(player, -1);
    CHECK(player.playhead == 28);
    replay_step(player, std::numeric_limits<int64_t>::min());
    CHECK(player.playhead == 0);
    replay_step(player, std::numeric_limits<int64_t>::max());
    CHECK(player.playhead == 29);
    replay_seek(player, 1u << 30);
    CHECK(player.playhead == 29);
    check_selection_follows(rig);

    replay_to_start(player);
    CHECK(player.playhead == 0);
    replay_to_end(player);
    CHECK(player.playhead == 29);
    check_selection_follows(rig);
}

TEST_CASE("replay_seek outside the window refills a window around the target")
{
    Rig rig(synthetic_log(3, 10));
    replay_seek(rig.player, 25);
    CHECK(rig.player.playhead == 25);
    RecordView view = rig.feed.find_sequence(25);
    REQUIRE(view.valid);
    CHECK(view.record->match_index == 2);
    CHECK(view.record->decision_index == 5);
    CHECK(rig.model.selected_sequence == 25);
    replay_seek(rig.player, 2);
    CHECK(rig.feed.find_sequence(2).valid);
    check_selection_follows(rig);
}

TEST_CASE("replay_previous_match and replay_next_match jump between matches and clamp at the ends")
{
    Rig rig(synthetic_log(3, 10));
    ReplayPlayer& player = rig.player;

    replay_seek(player, 15);
    replay_previous_match(player);
    CHECK(player.playhead == 10);
    replay_previous_match(player);
    CHECK(player.playhead == 0);
    replay_previous_match(player);
    CHECK(player.playhead == 0);

    replay_seek(player, 3);
    replay_next_match(player);
    CHECK(player.playhead == 10);
    replay_next_match(player);
    CHECK(player.playhead == 20);
    replay_next_match(player);
    CHECK(player.playhead == 29);
    replay_next_match(player);
    CHECK(player.playhead == 29);
    check_selection_follows(rig);
}

TEST_CASE("replay_tick advances at the fixed rate only while playing")
{
    Rig rig(synthetic_log(1, 20));
    ReplayPlayer& player = rig.player;
    player.rate = 2.0;

    replay_tick(player, 5.0);
    CHECK(player.playhead == 0);

    replay_play(player);
    replay_tick(player, 0.25);
    CHECK(player.playhead == 0);
    replay_tick(player, 0.25);
    CHECK(player.playhead == 1);
    replay_tick(player, 1.0);
    CHECK(player.playhead == 3);
    player.speed = 2.0;
    replay_tick(player, 1.0);
    CHECK(player.playhead == 7);
    replay_tick(player, -1.0);
    CHECK(player.playhead == 7);
    check_selection_follows(rig);

    replay_pause(player);
    replay_tick(player, 5.0);
    CHECK(player.playhead == 7);
    replay_toggle(player);
    CHECK(player.playing);
}

TEST_CASE("playing past the end pauses, restarts from the end on play, and loops when asked")
{
    Rig rig(synthetic_log(1, 6));
    ReplayPlayer& player = rig.player;
    player.rate = 1.0;

    replay_play(player);
    replay_tick(player, 100.0);
    CHECK(player.playhead == 5);
    CHECK_FALSE(player.playing);

    replay_play(player);
    CHECK(player.playhead == 0);
    CHECK(player.playing);

    player.loop = true;
    replay_tick(player, 100.0);
    CHECK(player.playing);
    CHECK(player.playhead == 0);
    replay_tick(player, 3.0);
    CHECK(player.playhead == 3);
}

TEST_CASE("real-time playback follows the recorded times and dwells on the last decision")
{
    Rig rig(join_lines({ header_line(), small_decision(0, 0, 0), small_decision(0, 1, 1000000), small_decision(0, 2, 3000000) }));
    ReplayPlayer& player = rig.player;
    player.rate = 2.0;

    replay_use_real_time(player, true);
    CHECK(player.clock == ReplayClock::RealTime);
    replay_play(player);
    replay_tick(player, 0.5);
    CHECK(player.playhead == 0);
    replay_tick(player, 0.5);
    CHECK(player.playhead == 1);
    replay_tick(player, 1.0);
    CHECK(player.playhead == 1);
    replay_tick(player, 1.0);
    CHECK(player.playhead == 2);
    replay_tick(player, 0.25);
    CHECK(player.playing);
    replay_tick(player, 0.5);
    CHECK_FALSE(player.playing);
    CHECK(player.playhead == 2);
}

TEST_CASE("real-time mode is refused for a log without times")
{
    Rig rig(synthetic_log(1, 4));
    replay_use_real_time(rig.player, true);
    CHECK(rig.player.clock == ReplayClock::FixedRate);
}

TEST_CASE("an empty log makes every player function a no-op")
{
    Rig rig(std::string(header_line()) + "\n");
    ReplayPlayer& player = rig.player;
    replay_play(player);
    CHECK_FALSE(player.playing);
    replay_step(player, 3);
    replay_seek(player, 7);
    replay_to_end(player);
    replay_to_start(player);
    replay_previous_match(player);
    replay_next_match(player);
    replay_tick(player, 1.0);
    CHECK(player.playhead == 0);
    CHECK(replay_length(player) == 0);
    CHECK(rig.feed.size() == 0);
    CHECK_FALSE(selected_view(rig.model).valid);
}
