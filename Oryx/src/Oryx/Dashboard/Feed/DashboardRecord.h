#pragma once

#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/PlayerId.h"
#include "Oryx/Strategy/Observability/ActionDescription.h"
#include "Oryx/Strategy/Observability/Decision.h"

namespace oryx
{

struct ScoreEntry
{
    ActionId action = INVALID_ACTION;
    double probability = 0.0;
    double value = 0.0;
    bool has_probability = false;
    bool has_value = false;
    uint8_t space_count = 0;
    uint32_t spaces[k_record_spaces] = {};
    FixedLabel label;
};

struct MetricEntry
{
    uint16_t key = 0;
    double value = 0.0;
};

// chosen_value/chosen_probability are float so GUI Values can stride over a record array; NaN means absent and draws as a gap.
struct DashboardRecord
{
    uint64_t sequence = 0;
    uint32_t match_index = 0;
    uint32_t decision_index = 0;
    PlayerId player = 0;
    ActionId chosen = INVALID_ACTION;
    float chosen_value = std::numeric_limits<float>::quiet_NaN();
    float chosen_probability = std::numeric_limits<float>::quiet_NaN();
    uint32_t score_count = 0;
    uint32_t metric_count = 0;
    uint32_t tree_count = 0;
    uint32_t dropped_scores = 0;
    uint32_t dropped_metrics = 0;
    uint32_t dropped_tree_nodes = 0;
    // Microseconds since the match started; -1 when the source did not time it (live feed, untimed logs).
    int64_t time_us = -1;
    uint8_t chosen_space_count = 0;
    uint32_t chosen_spaces[k_record_spaces] = {};
    FixedLabel chosen_label;
};

static_assert(std::is_trivially_copyable_v<DashboardRecord> && std::is_standard_layout_v<DashboardRecord>);
static_assert(std::is_trivially_copyable_v<ScoreEntry> && std::is_trivially_copyable_v<MetricEntry>);

struct KeyStats
{
    double sum = 0.0;
    double max = 0.0;
    double last = 0.0;
    int64_t count = 0;
    bool keeps_max = false;
};

struct FeedDrops
{
    uint64_t scores = 0;
    uint64_t metrics = 0;
    uint64_t tree_nodes = 0;
};

// Pointers and counts into the feed's pools; valid until the next on_decision, clear or reset.
struct RecordView
{
    const DashboardRecord* record = nullptr;
    const ScoreEntry* scores = nullptr;
    const MetricEntry* metrics = nullptr;
    const SearchNode* tree = nullptr;
    bool valid = false;
};

// The text before the first '/', empty when the key has none.
[[nodiscard]] inline std::string_view key_namespace(std::string_view key)
{
    size_t slash = key.find('/');
    return slash == std::string_view::npos ? std::string_view() : key.substr(0, slash);
}

} // namespace oryx
