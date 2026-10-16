#pragma once

#include "Oryx.h"
#include "unit/MemoryTestSupport.h"

namespace oryx::test
{

// A log file in the temp directory, removed on destruction.
class TempLog
{
public:
    explicit TempLog(const std::string& content = std::string())
    {
        static uint32_t counter = 0;
        uint64_t stamp = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
        m_path = std::filesystem::temp_directory_path() / ("oryx-replay-" + std::to_string(stamp) + "-" + std::to_string(++counter) + ".jsonl");
        write(content);
    }
    ~TempLog()
    {
        std::error_code ignored;
        std::filesystem::remove(m_path, ignored);
    }
    TempLog(const TempLog&) = delete;
    TempLog& operator=(const TempLog&) = delete;

    void write(const std::string& content) const
    {
        std::ofstream out(m_path, std::ios::binary | std::ios::trunc);
        out << content;
    }
    void append(const std::string& content) const
    {
        std::ofstream out(m_path, std::ios::binary | std::ios::app);
        out << content;
    }
    [[nodiscard]] std::string path() const { return m_path.string(); }

private:
    std::filesystem::path m_path;
};

inline const char* header_line()
{
    return R"({"kind":"header","schema_version":1,"game":"tictactoe","params":{"size":3,"misere":false,"komi":0.5,"name":"x"},"players":["a","b"],"seed":7,"build":"v"})";
}

inline std::string full_decision(int32_t match, int32_t ply)
{
    return R"({"kind":"decision","match":)" + std::to_string(match) + R"(,"ply":)" + std::to_string(ply) +
           R"(,"player":0,"chosen":4,"chosen_label":"c4","chosen_spaces":[4],"scores":[{"action":4,"label":"c4","spaces":[4],"probability":0.5,"value":1.5},{"action":5,"label":"c5","value":-1}],"extra":{"minimax/nodes":10,"depth_max":3},"tree":[{"parent":-1,"action":4294967295,"visits":10,"value":0.25},{"parent":0,"action":4,"visits":5,"value":0.5}]})";
}

inline std::string small_decision(int32_t match, int32_t ply, int64_t time_us = -1)
{
    std::string line = R"({"kind":"decision","match":)" + std::to_string(match) + R"(,"ply":)" + std::to_string(ply) + R"(,"player":0,"chosen":1,"chosen_label":"a")";
    if (time_us >= 0)
    {
        line += R"(,"t_us":)" + std::to_string(time_us);
    }
    return line + R"(,"scores":[],"extra":{},"tree":[]})";
}

inline std::string join_lines(const std::vector<std::string>& lines, const char* ending = "\n")
{
    std::string text;
    for (const std::string& line : lines)
    {
        text += line + ending;
    }
    return text;
}

// matches x per_match small decisions behind the standard header.
inline std::string synthetic_log(int32_t matches, int32_t per_match)
{
    std::string text = std::string(header_line()) + "\n";
    for (int32_t match = 0; match < matches; ++match)
    {
        for (int32_t ply = 0; ply < per_match; ++ply)
        {
            text += small_decision(match, ply) + "\n";
        }
    }
    return text;
}

struct Pools
{
    explicit Pools(uint32_t scores = 64, uint32_t metrics = 32, uint32_t tree = 64)
        : score_pool(scores)
        , metric_pool(metrics)
        , tree_pool(tree)
    {
        parsed.scores = score_pool.data();
        parsed.score_capacity = scores;
        parsed.metrics = metric_pool.data();
        parsed.metric_capacity = metrics;
        parsed.tree = tree_pool.data();
        parsed.tree_capacity = tree;
    }

    std::vector<ScoreEntry> score_pool;
    std::vector<MetricEntry> metric_pool;
    std::vector<SearchNode> tree_pool;
    ParsedRecord parsed;
};

} // namespace oryx::test
