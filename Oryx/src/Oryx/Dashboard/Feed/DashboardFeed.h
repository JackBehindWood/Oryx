#pragma once

#include "Oryx/Dashboard/Feed/DashboardRecord.h"
#include "Oryx/Dashboard/Feed/FeedOptions.h"
#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

// Bounded ring of self-contained decision records; allocates only in the constructor, reset() and the first sighting of a key.
class DashboardFeed : public IDecisionObserver
{
public:
    explicit DashboardFeed(const FeedOptions& options = FeedOptions());
    DashboardFeed(const DashboardFeed&) = delete;
    DashboardFeed& operator=(const DashboardFeed&) = delete;

    void on_decision(const IState& state, const Decision& decision) override;
    void on_match_start() override { begin_match(); }

    void reset(const FeedOptions& options);
    void clear();
    void begin_match();
    // The next begin_match clears the ring first, so a switch to another game starts empty even if the old match decides once more before it ends.
    void clear_at_next_match() { m_clear_at_next_match = true; }

    [[nodiscard]] const FeedOptions& options() const { return m_options; }
    [[nodiscard]] size_t size() const { return m_total < m_records.size() ? static_cast<size_t>(m_total) : m_records.size(); }
    [[nodiscard]] size_t capacity() const { return m_records.size(); }
    [[nodiscard]] uint64_t total_decisions() const { return m_total; }
    [[nodiscard]] uint32_t match_index() const { return m_match_index; }
    [[nodiscard]] const FeedDrops& drops() const { return m_drops; }

    // Index 0 is the oldest record still held.
    [[nodiscard]] RecordView view(size_t index) const;
    [[nodiscard]] RecordView find_sequence(uint64_t sequence) const;

    // The record array and the slot of the oldest record, for GUI Values{ data, size, first_slot, stride }.
    [[nodiscard]] const DashboardRecord* data() const { return m_records.data(); }
    [[nodiscard]] size_t first_slot() const { return m_total <= m_records.size() ? 0 : static_cast<size_t>(m_total % m_records.size()); }

    [[nodiscard]] size_t key_count() const { return m_key_names.size(); }
    [[nodiscard]] std::string_view key_name(uint16_t key) const { return m_key_names[key]; }
    [[nodiscard]] const KeyStats& key_stats(uint16_t key) const { return m_key_stats[key]; }
    [[nodiscard]] int32_t find_key(std::string_view name) const;

private:
    size_t store_numbers(const Decision& decision);
    void store_labels(size_t slot, const IState& state, const Decision& decision);
    int32_t intern_key(const std::string& name);
    [[nodiscard]] RecordView view_of_slot(size_t slot) const;

    FeedOptions m_options;
    std::vector<DashboardRecord> m_records;
    std::vector<ScoreEntry> m_scores;
    std::vector<MetricEntry> m_metrics;
    std::vector<SearchNode> m_tree;
    std::vector<std::string> m_key_names;
    std::vector<KeyStats> m_key_stats;
    FeedDrops m_drops;
    uint64_t m_total = 0;
    bool m_clear_at_next_match = false;
    uint64_t m_base_sequence = 0;
    uint32_t m_match_index = 0;
    uint32_t m_decision_index = 0;
};

} // namespace oryx
