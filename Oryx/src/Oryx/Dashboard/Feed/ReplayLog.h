#pragma once

#include "Oryx/Core/Params.h"
#include "Oryx/Dashboard/Feed/DashboardRecord.h"
#include "Oryx/Dashboard/Feed/FeedOptions.h"

namespace oryx
{

struct ReplayOptions
{
    uint64_t max_records = 10'000'000;
    int32_t max_keys = k_max_feed_keys;
    uint32_t max_line_bytes = 1u << 20;
    // Nesting cap for any value in a line, 8..64.
    uint32_t max_depth = 32;
};

struct ReplayHeader
{
    std::string game;
    Params params;
    std::vector<std::string> players;
    uint64_t seed = 0;
    std::string build;
};

struct ReplayStringHash
{
    using is_transparent = void;
    size_t operator()(std::string_view text) const { return std::hash<std::string_view>()(text); }
};

// The log-global metric keys; ids are positions in `names` and stay stable across window refills.
struct ReplayKeyTable
{
    using Ids = std::unordered_map<std::string, uint32_t, ReplayStringHash, std::equal_to<>>;
    Ids ids;
    std::vector<std::string> names;
    std::vector<KeyStats> stats;
    size_t max_keys = 0;
    // Only the open pass adds keys; read() looks them up.
    bool interning = false;
};

// A record plus the caller-owned pools it fills; a capacity of 0 still counts what it would have kept.
struct ParsedRecord
{
    DashboardRecord record;
    ScoreEntry* scores = nullptr;
    uint32_t score_capacity = 0;
    MetricEntry* metrics = nullptr;
    uint32_t metric_capacity = 0;
    SearchNode* tree = nullptr;
    uint32_t tree_capacity = 0;
};

// A trace log on disk: open() streams it once into a line-offset index (8 B per decision) plus whole-log key tables; read() fetches one decision without allocating. Single-threaded.
class ReplayLog
{
public:
    ReplayLog();
    ~ReplayLog();
    ReplayLog(const ReplayLog&) = delete;
    ReplayLog& operator=(const ReplayLog&) = delete;

    // Throws oryx::Error for a missing file, a missing or malformed header or a newer schema_version; bad lines after the header are skipped and counted.
    void open(const std::string& path, const ReplayOptions& options = ReplayOptions());
    void close();
    // Indexes lines appended since the last open/refresh, including a last line that was unterminated before.
    void refresh();

    // False when the line cannot be read or no longer scans (counted, never thrown); sequence is left for the caller to set.
    [[nodiscard]] bool read(uint64_t index, ParsedRecord& out);

    [[nodiscard]] bool is_open() const { return m_file != nullptr; }
    [[nodiscard]] const ReplayHeader& header() const { return m_header; }
    [[nodiscard]] uint64_t record_count() const { return m_offsets.size(); }
    [[nodiscard]] uint64_t match_count() const { return m_match_starts.size(); }
    [[nodiscard]] uint64_t match_start(uint64_t match) const { return m_match_starts[match]; }
    // The match's ordinal in the log (0-based, by first appearance), not the value written on the line.
    [[nodiscard]] uint64_t match_of(uint64_t index) const;
    [[nodiscard]] bool has_time() const { return m_has_time; }

    [[nodiscard]] uint64_t malformed_count() const { return m_malformed; }
    [[nodiscard]] uint64_t oversize_count() const { return m_oversize; }
    [[nodiscard]] uint64_t read_errors() const { return m_read_errors; }
    // The record cap stopped indexing.
    [[nodiscard]] bool truncated() const { return m_truncated; }
    // A final decision line with no newline yet, not indexed.
    [[nodiscard]] bool incomplete_tail() const { return m_incomplete_tail; }

    [[nodiscard]] size_t key_count() const { return m_keys.names.size(); }
    [[nodiscard]] std::string_view key_name(uint16_t key) const { return m_keys.names[key]; }
    [[nodiscard]] const KeyStats& key_stats(uint16_t key) const { return m_keys.stats[key]; }

    // Largest counts any one decision carries (kept or not), for sizing a feed's pools.
    [[nodiscard]] uint32_t max_scores() const { return m_max_scores; }
    [[nodiscard]] uint32_t max_metrics() const { return m_max_metrics; }
    [[nodiscard]] uint32_t max_tree_nodes() const { return m_max_tree_nodes; }

private:
    void index_from(uint64_t offset);
    void handle_line(uint64_t offset, std::string& line);

    std::FILE* m_file = nullptr;
    ReplayOptions m_options;
    ReplayHeader m_header;
    std::vector<uint64_t> m_offsets;
    std::vector<uint64_t> m_match_starts;
    ReplayKeyTable m_keys;
    std::vector<MetricEntry> m_scratch_metrics;
    std::vector<char> m_block;
    std::string m_line;
    uint64_t m_committed = 0;
    size_t m_longest_line = 0;
    int64_t m_last_match = -1;
    uint64_t m_malformed = 0;
    uint64_t m_oversize = 0;
    uint64_t m_read_errors = 0;
    uint32_t m_max_scores = 0;
    uint32_t m_max_metrics = 0;
    uint32_t m_max_tree_nodes = 0;
    bool m_truncated = false;
    bool m_incomplete_tail = false;
    bool m_has_time = false;
    bool m_header_done = false;
};

} // namespace oryx
