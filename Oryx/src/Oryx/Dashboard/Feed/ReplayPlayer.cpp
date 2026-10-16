#include "ReplayPlayer.h"

namespace oryx
{

namespace
{

struct Stamp
{
    uint32_t match = 0;
    int64_t time_us = -1;
};

void ensure_resident(ReplayPlayer& player, uint64_t index)
{
    if (index < player.feed->oldest_sequence() || index >= player.feed->total_decisions())
    {
        uint64_t lead = player.feed->capacity() / 4;
        player.feed->load_window(*player.log, index > lead ? index - lead : 0);
    }
}

Stamp stamp_of(ReplayPlayer& player, uint64_t index)
{
    ensure_resident(player, index);
    RecordView view = player.feed->find_sequence(index);
    return view.valid ? Stamp{ view.record->match_index, view.record->time_us } : Stamp{};
}

double seconds_us(const Stamp& stamp)
{
    return stamp.time_us > 0 ? static_cast<double>(stamp.time_us) : 0.0;
}

void sync_clock(ReplayPlayer& player)
{
    player.match_clock_us = seconds_us(stamp_of(player, player.playhead));
    player.accumulator = 0.0;
}

void finish(ReplayPlayer& player)
{
    if (player.loop)
    {
        replay_to_start(player);
    }
    else
    {
        player.playing = false;
    }
}

void tick_real_time(ReplayPlayer& player, uint64_t length)
{
    constexpr uint64_t k_max_steps_per_tick = 1u << 20;
    double dwell_us = 1e6 / math::max(player.rate, 1e-3);
    Stamp current = stamp_of(player, player.playhead);
    for (uint64_t steps = 0; steps < k_max_steps_per_tick && player.playing; ++steps)
    {
        double current_us = seconds_us(current);
        if (player.playhead + 1 >= length)
        {
            if (player.match_clock_us >= current_us + dwell_us)
            {
                finish(player);
            }
            return;
        }
        Stamp next = stamp_of(player, player.playhead + 1);
        bool same_match = next.match == current.match;
        double due_us = same_match && next.time_us >= 0 ? static_cast<double>(next.time_us) : current_us + dwell_us;
        if (player.match_clock_us < due_us)
        {
            return;
        }
        replay_seek(player, player.playhead + 1);
        if (!same_match)
        {
            player.match_clock_us = seconds_us(next);
        }
        current = next;
    }
}

} // namespace

FeedOptions replay_feed_options(const ReplayLog& log, const ReplayLimits& limits)
{
    FeedOptions options;
    options.capacity = static_cast<int32_t>(math::clamp<uint64_t>(log.record_count(), 1, static_cast<uint64_t>(math::max(limits.window, 1))));
    options.max_scores = static_cast<int32_t>(math::clamp<uint32_t>(log.max_scores(), 1, k_replay_max_scores));
    options.max_metrics = static_cast<int32_t>(math::clamp<uint32_t>(log.max_metrics(), 1, k_replay_max_metrics));
    options.max_tree_nodes = static_cast<int32_t>(math::min<uint32_t>(log.max_tree_nodes(), static_cast<uint32_t>(math::max(limits.max_tree_nodes, 0))));
    options.max_keys = static_cast<int32_t>(math::clamp<size_t>(log.key_count(), 1, static_cast<size_t>(k_max_feed_keys)));
    return options;
}

void replay_open(ReplayPlayer& player, ReplayLog& log, DashboardFeed& feed, DashboardModel& model, const ReplayLimits& limits)
{
    player.log = &log;
    player.feed = &feed;
    player.model = &model;
    player.playhead = 0;
    player.playing = false;
    player.accumulator = 0.0;
    player.match_clock_us = 0.0;
    if (!log.has_time())
    {
        player.clock = ReplayClock::FixedRate;
    }
    feed.reset(replay_feed_options(log, limits));
    feed.adopt_keys(log);
    feed.load_window(log, 0);
    model.feed = &feed;
    if (log.record_count() > 0)
    {
        select_record(model, 0);
        sync_clock(player);
    }
}

void replay_seek(ReplayPlayer& player, uint64_t index)
{
    uint64_t length = replay_length(player);
    if (length == 0)
    {
        return;
    }
    index = math::min(index, length - 1);
    ensure_resident(player, index);
    player.playhead = index;
    select_record(*player.model, index);
}

void replay_step(ReplayPlayer& player, int64_t delta)
{
    uint64_t magnitude = delta < 0 ? static_cast<uint64_t>(-(delta + 1)) + 1 : static_cast<uint64_t>(delta);
    if (delta < 0)
    {
        replay_seek(player, magnitude > player.playhead ? 0 : player.playhead - magnitude);
    }
    else
    {
        replay_seek(player, magnitude > UINT64_MAX - player.playhead ? UINT64_MAX : player.playhead + magnitude);
    }
    sync_clock(player);
}

void replay_to_start(ReplayPlayer& player)
{
    replay_seek(player, 0);
    sync_clock(player);
}

void replay_to_end(ReplayPlayer& player)
{
    uint64_t length = replay_length(player);
    replay_seek(player, length == 0 ? 0 : length - 1);
    sync_clock(player);
}

void replay_previous_match(ReplayPlayer& player)
{
    if (replay_length(player) == 0)
    {
        return;
    }
    uint64_t match = player.log->match_of(player.playhead);
    uint64_t start = player.log->match_start(match);
    replay_seek(player, player.playhead > start ? start : (match > 0 ? player.log->match_start(match - 1) : 0));
    sync_clock(player);
}

void replay_next_match(ReplayPlayer& player)
{
    uint64_t length = replay_length(player);
    if (length == 0)
    {
        return;
    }
    uint64_t match = player.log->match_of(player.playhead);
    replay_seek(player, match + 1 < player.log->match_count() ? player.log->match_start(match + 1) : length - 1);
    sync_clock(player);
}

void replay_play(ReplayPlayer& player)
{
    uint64_t length = replay_length(player);
    if (length == 0)
    {
        return;
    }
    if (player.playhead + 1 >= length && !player.loop)
    {
        replay_seek(player, 0);
    }
    sync_clock(player);
    player.playing = true;
}

void replay_pause(ReplayPlayer& player)
{
    player.playing = false;
}

void replay_toggle(ReplayPlayer& player)
{
    if (player.playing)
    {
        replay_pause(player);
    }
    else
    {
        replay_play(player);
    }
}

void replay_use_real_time(ReplayPlayer& player, bool real_time)
{
    player.clock = real_time && player.log != nullptr && player.log->has_time() ? ReplayClock::RealTime : ReplayClock::FixedRate;
    sync_clock(player);
}

void replay_tick(ReplayPlayer& player, double seconds)
{
    uint64_t length = replay_length(player);
    if (!player.playing || length == 0 || !(seconds > 0.0))
    {
        return;
    }
    if (player.clock == ReplayClock::RealTime)
    {
        player.match_clock_us += seconds * 1e6 * player.speed;
        tick_real_time(player, length);
        return;
    }
    player.accumulator += seconds * player.rate * player.speed;
    double whole = std::floor(player.accumulator);
    if (whole < 1.0)
    {
        return;
    }
    player.accumulator -= whole;
    uint64_t steps = whole >= 1e15 ? static_cast<uint64_t>(1e15) : static_cast<uint64_t>(whole);
    uint64_t remaining = length - 1 - player.playhead;
    if (steps > remaining)
    {
        replay_seek(player, length - 1);
        finish(player);
        return;
    }
    replay_seek(player, player.playhead + steps);
}

} // namespace oryx
