#pragma once

#include "Oryx/Dashboard/Feed/DashboardModel.h"
#include "Oryx/Dashboard/Feed/ReplayLog.h"

namespace oryx
{

// Per-record pool caps so a window of a wide log stays bounded (4096 x 128 scores is about 46 MB).
constexpr int32_t k_replay_max_scores = 128;
constexpr int32_t k_replay_max_metrics = 64;

enum class ReplayClock : uint8_t
{
    FixedRate,
    // Follows the recorded t_us; only meaningful when the log has times.
    RealTime
};

struct ReplayLimits
{
    int32_t window = 4096;
    int32_t max_tree_nodes = 256;
};

// The log, the feed window and the shared selection, driven by free functions; no globals, so several players can coexist.
struct ReplayPlayer
{
    ReplayLog* log = nullptr;
    DashboardFeed* feed = nullptr;
    DashboardModel* model = nullptr;
    uint64_t playhead = 0;
    bool playing = false;
    bool loop = false;
    ReplayClock clock = ReplayClock::FixedRate;
    // Decisions per second at speed 1 in FixedRate mode; also the dwell on a record that has no time to follow.
    double rate = 2.0;
    double speed = 1.0;
    double accumulator = 0.0;
    double match_clock_us = 0.0;
};

// Pools sized to the log's maxima, capped, and capacity = min(records, window).
[[nodiscard]] FeedOptions replay_feed_options(const ReplayLog& log, const ReplayLimits& limits);

// Resets the feed to the log's shape, loads the first window and selects the first decision; every later call clamps and never throws.
void replay_open(ReplayPlayer& player, ReplayLog& log, DashboardFeed& feed, DashboardModel& model, const ReplayLimits& limits);

void replay_play(ReplayPlayer& player);
void replay_pause(ReplayPlayer& player);
void replay_toggle(ReplayPlayer& player);
void replay_seek(ReplayPlayer& player, uint64_t index);
void replay_step(ReplayPlayer& player, int64_t delta);
void replay_to_start(ReplayPlayer& player);
void replay_to_end(ReplayPlayer& player);
// Inside a match this goes to its first decision; at that decision, to the previous match's.
void replay_previous_match(ReplayPlayer& player);
// The last match has no next one, so this lands on the final decision.
void replay_next_match(ReplayPlayer& player);
// Falls back to FixedRate when the log carries no times.
void replay_use_real_time(ReplayPlayer& player, bool real_time);
void replay_tick(ReplayPlayer& player, double seconds);

[[nodiscard]] inline uint64_t replay_length(const ReplayPlayer& player) { return player.log == nullptr ? 0 : player.log->record_count(); }

} // namespace oryx
