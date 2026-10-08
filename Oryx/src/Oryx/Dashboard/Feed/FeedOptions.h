#pragma once

namespace oryx
{

// Every limit is a fixed pool size chosen once, so the feed never allocates per decision; overflow is counted, not thrown.
struct FeedOptions
{
    int32_t capacity = 256;
    int32_t max_scores = 64;
    int32_t max_metrics = 32;
    int32_t max_keys = 128;
    int32_t max_tree_nodes = 0;
};

constexpr int32_t k_max_feed_keys = 65535;

} // namespace oryx
