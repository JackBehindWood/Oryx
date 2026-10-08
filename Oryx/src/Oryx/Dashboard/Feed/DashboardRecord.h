#pragma once

#include "Oryx/Game/ActionId.h"
#include "Oryx/Game/PlayerId.h"
#include "Oryx/Strategy/Observability/Decision.h"

namespace oryx
{

constexpr size_t k_label_capacity = 24;

struct FixedLabel
{
    char text[k_label_capacity] = {};
    uint8_t length = 0;
};

inline void set_label(FixedLabel& label, std::string_view text)
{
    size_t length = text.size() < k_label_capacity ? text.size() : k_label_capacity;
    // Never cut inside a multi-byte UTF-8 sequence.
    while (length < text.size() && length > 0 && (static_cast<uint8_t>(text[length]) & 0xC0) == 0x80)
    {
        --length;
    }
    std::memcpy(label.text, text.data(), length);
    label.length = static_cast<uint8_t>(length);
}

[[nodiscard]] inline std::string_view label_view(const FixedLabel& label)
{
    return std::string_view(label.text, label.length);
}

struct ScoreEntry
{
    ActionId action = INVALID_ACTION;
    double probability = 0.0;
    double value = 0.0;
    bool has_probability = false;
    bool has_value = false;
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
