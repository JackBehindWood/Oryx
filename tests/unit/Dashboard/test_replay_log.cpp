#include "doctest.h"

#include "ReplayTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

std::string with_extra(const std::string& line, const std::string& extra)
{
    return line.substr(0, line.size() - 1) + "," + extra + "}";
}

} // namespace

TEST_CASE("ReplayLog opens a log, exposes the header and reads a full record")
{
    TempLog file(join_lines({ header_line(), full_decision(0, 0), full_decision(0, 1) }));
    ReplayLog log;
    log.open(file.path());

    CHECK(log.header().game == "tictactoe");
    CHECK(log.header().players == std::vector<std::string>{ "a", "b" });
    CHECK(log.header().seed == 7);
    CHECK(log.header().build == "v");
    CHECK(std::get<int64_t>(log.header().params.at("size")) == 3);
    CHECK(std::get<bool>(log.header().params.at("misere")) == false);
    CHECK(std::get<double>(log.header().params.at("komi")) == 0.5);
    CHECK(std::get<std::string>(log.header().params.at("name")) == "x");
    REQUIRE(log.record_count() == 2);
    CHECK(log.match_count() == 1);
    CHECK_FALSE(log.has_time());
    CHECK(log.key_count() == 2);
    CHECK(log.key_name(0) == "minimax/nodes");
    CHECK(log.key_stats(0).count == 2);
    CHECK(log.key_stats(0).sum == 20);
    CHECK(log.key_stats(1).keeps_max);
    CHECK(log.max_scores() == 2);
    CHECK(log.max_metrics() == 2);
    CHECK(log.max_tree_nodes() == 2);

    Pools pools;
    REQUIRE(log.read(1, pools.parsed));
    const DashboardRecord& record = pools.parsed.record;
    CHECK(record.match_index == 0);
    CHECK(record.decision_index == 1);
    CHECK(record.chosen == 4);
    CHECK(label_view(record.chosen_label) == "c4");
    REQUIRE(record.chosen_space_count == 1);
    CHECK(record.chosen_spaces[0] == 4);
    CHECK(record.chosen_value == 1.5f);
    CHECK(record.chosen_probability == 0.5f);
    CHECK(record.time_us == -1);
    REQUIRE(record.score_count == 2);
    CHECK(pools.score_pool[0].has_probability);
    CHECK(pools.score_pool[1].has_value);
    CHECK_FALSE(pools.score_pool[1].has_probability);
    CHECK(pools.score_pool[1].value == -1.0);
    CHECK(label_view(pools.score_pool[1].label) == "c5");
    REQUIRE(record.metric_count == 2);
    CHECK(pools.metric_pool[1].key == 1);
    CHECK(pools.metric_pool[1].value == 3.0);
    REQUIRE(record.tree_count == 2);
    CHECK(pools.tree_pool[0].action == INVALID_ACTION);
    CHECK(pools.tree_pool[1].parent == 0);
    CHECK(pools.tree_pool[1].visits == 5);
}

TEST_CASE("ReplayLog rejects a missing, empty or invalid header with an Error")
{
    ReplayLog log;
    CHECK_THROWS_AS(log.open("/nonexistent/oryx-replay.jsonl"), Error);
    for (const std::string& text : std::vector<std::string>{ "", "\n\n", header_line(), "{}\n", "not json\n", full_decision(0, 0) + "\n", R"({"kind":"header","game":"x"})" "\n",
                                                           R"({"kind":"header","schema_version":0})" "\n", R"({"kind":"header","schema_version":2})" "\n", R"({"kind":"header","schema_version":99999999999})" "\n" })
    {
        INFO("log: " << text);
        TempLog file(text);
        CHECK_THROWS_AS(log.open(file.path()), Error);
        CHECK_FALSE(log.is_open());
    }
}

TEST_CASE("ReplayLog accepts a header-only log with no records")
{
    TempLog file(std::string(header_line()) + "\n");
    ReplayLog log;
    log.open(file.path());
    CHECK(log.record_count() == 0);
    CHECK(log.match_count() == 0);
    Pools pools;
    CHECK_FALSE(log.read(0, pools.parsed));
}

TEST_CASE("ReplayLog opens a log truncated at every byte offset with exactly its complete prefix")
{
    std::string text = join_lines({ header_line(), full_decision(0, 0), small_decision(0, 1), full_decision(1, 0) });
    size_t header_end = text.find('\n') + 1;
    ReplayLog log;
    for (size_t cut = 0; cut <= text.size(); ++cut)
    {
        TempLog file(text.substr(0, cut));
        if (cut < header_end)
        {
            CHECK_THROWS_AS(log.open(file.path()), Error);
            continue;
        }
        size_t complete = static_cast<size_t>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(cut), '\n')) - 1;
        INFO("cut at " << cut);
        REQUIRE_NOTHROW(log.open(file.path()));
        CHECK(log.record_count() == complete);
        CHECK(log.malformed_count() == 0);
        CHECK(log.incomplete_tail() == (cut > 0 && text[cut - 1] != '\n'));
    }
}

TEST_CASE("ReplayLog refresh picks up appended lines and completes a half-written one")
{
    std::string second = small_decision(0, 1);
    TempLog file(join_lines({ header_line(), small_decision(0, 0) }) + second.substr(0, 20));
    ReplayLog log;
    log.open(file.path());
    CHECK(log.record_count() == 1);
    CHECK(log.incomplete_tail());

    file.append(second.substr(20) + "\n");
    log.refresh();
    CHECK(log.record_count() == 2);
    CHECK_FALSE(log.incomplete_tail());

    file.append(small_decision(1, 0) + "\n" + small_decision(1, 1) + "\n");
    log.refresh();
    CHECK(log.record_count() == 4);
    CHECK(log.match_count() == 2);
    Pools pools;
    REQUIRE(log.read(3, pools.parsed));
    CHECK(pools.parsed.record.match_index == 1);
    CHECK(pools.parsed.record.decision_index == 1);
}

TEST_CASE("ReplayLog skips and counts malformed and oversize lines, and unknown kinds silently")
{
    std::string oversize = with_extra(small_decision(0, 1), "\"pad\":\"" + std::string(400, 'x') + "\"");
    TempLog file(join_lines({ header_line(),
                              small_decision(0, 0),
                              R"({"kind":"decision","match":0,)",
                              oversize,
                              R"({"kind":"decision","chosen":-1})",
                              R"({"kind":"decision","chosen":"x"})",
                              R"({"kind":"match_end","outcome":"win"})",
                              "",
                              "   ",
                              small_decision(0, 2) }));
    ReplayLog log;
    ReplayOptions options;
    options.max_line_bytes = 256;
    log.open(file.path(), options);
    CHECK(log.record_count() == 2);
    CHECK(log.malformed_count() == 3);
    CHECK(log.oversize_count() == 1);

    Pools pools;
    REQUIRE(log.read(1, pools.parsed));
    CHECK(pools.parsed.record.decision_index == 2);
}

TEST_CASE("ReplayLog tolerates CRLF line endings and a UTF-8 byte order mark")
{
    TempLog file("\xEF\xBB\xBF" + join_lines({ header_line(), small_decision(0, 0), small_decision(0, 1) }, "\r\n"));
    ReplayLog log;
    log.open(file.path());
    REQUIRE(log.record_count() == 2);
    CHECK(log.malformed_count() == 0);
    Pools pools;
    REQUIRE(log.read(1, pools.parsed));
    CHECK(pools.parsed.record.decision_index == 1);
}

TEST_CASE("ReplayLog reads null numbers as NaN, escapes and UTF-8 as text, and ignores unknown fields")
{
    std::string line = R"({"kind":"decision","future":{"a":[1,2,{"b":"}]"}],"c":null},"match":0,"ply":0,"player":1,"chosen":3,)"
                       R"("chosen_label":"a\"b\\cé😀\ud800","scores":[{"action":3,"label":"é","value":null,"probability":null,"extra_field":[1]}],)"
                       R"("extra":{"nA":null,"k":2},"tree":[{"parent":-1,"action":3,"visits":2,"value":null,"more":{}}],"after":"text"})";
    TempLog file(join_lines({ header_line(), line }));
    ReplayLog log;
    log.open(file.path());
    REQUIRE(log.record_count() == 1);
    CHECK(log.malformed_count() == 0);
    CHECK(log.key_count() == 2);
    CHECK(log.key_name(0) == "nA");

    Pools pools;
    REQUIRE(log.read(0, pools.parsed));
    const DashboardRecord& record = pools.parsed.record;
    CHECK(record.player == 1);
    CHECK(label_view(record.chosen_label) == "a\"b\\c\xC3\xA9\xF0\x9F\x98\x80\xEF\xBF\xBD");
    REQUIRE(record.score_count == 1);
    CHECK(label_view(pools.score_pool[0].label) == "\xC3\xA9");
    CHECK(pools.score_pool[0].has_value);
    CHECK(std::isnan(pools.score_pool[0].value));
    CHECK(pools.score_pool[0].has_probability);
    CHECK(std::isnan(record.chosen_value));
    CHECK(std::isnan(record.chosen_probability));
    REQUIRE(record.metric_count == 2);
    CHECK(std::isnan(pools.metric_pool[0].value));
    CHECK(pools.metric_pool[1].value == 2.0);
    REQUIRE(record.tree_count == 1);
    CHECK(std::isnan(pools.tree_pool[0].value));
}

TEST_CASE("ReplayLog counts what a record carries beyond the pools and what a full key table refuses")
{
    TempLog file(join_lines({ header_line(), full_decision(0, 0) }));
    ReplayLog log;
    log.open(file.path());

    Pools small(1, 1, 1);
    REQUIRE(log.read(0, small.parsed));
    const DashboardRecord& record = small.parsed.record;
    CHECK(record.score_count == 1);
    CHECK(record.dropped_scores == 1);
    CHECK(record.metric_count == 1);
    CHECK(record.dropped_metrics == 1);
    CHECK(record.tree_count == 1);
    CHECK(record.dropped_tree_nodes == 1);
    CHECK(record.chosen_value == 1.5f);

    ReplayOptions options;
    options.max_keys = 1;
    ReplayLog narrow;
    narrow.open(file.path(), options);
    CHECK(narrow.key_count() == 1);
    Pools pools;
    REQUIRE(narrow.read(0, pools.parsed));
    CHECK(pools.parsed.record.metric_count == 1);
    CHECK(pools.parsed.record.dropped_metrics == 1);
}

TEST_CASE("ReplayLog keeps the chosen score's numbers when chosen follows scores")
{
    TempLog file(join_lines({ header_line(), R"({"kind":"decision","match":0,"ply":0,"scores":[{"action":2,"value":9},{"action":4,"value":7,"probability":0.25}],"chosen":4})" }));
    ReplayLog log;
    log.open(file.path());
    Pools pools;
    REQUIRE(log.read(0, pools.parsed));
    CHECK(pools.parsed.record.chosen_value == 7.0f);
    CHECK(pools.parsed.record.chosen_probability == 0.25f);
}

TEST_CASE("ReplayLog survives hostile nesting without recursing")
{
    std::string deep(100000, '[');
    std::string unbalanced = with_extra(small_decision(0, 1), "\"deep\":" + deep);
    std::string mismatched = with_extra(small_decision(0, 2), R"("deep":[{]})");
    std::string within_cap = with_extra(small_decision(0, 3), "\"deep\":" + std::string(20, '[') + std::string(20, ']'));
    std::string over_cap = with_extra(small_decision(0, 4), "\"deep\":" + std::string(60, '[') + std::string(60, ']'));
    TempLog file(join_lines({ header_line(), small_decision(0, 0), unbalanced, mismatched, within_cap, over_cap }));
    ReplayLog log;
    log.open(file.path());
    CHECK(log.record_count() == 2);
    CHECK(log.malformed_count() == 3);
}

TEST_CASE("ReplayLog stops indexing at the record cap and says so")
{
    TempLog file(synthetic_log(1, 10));
    ReplayLog log;
    ReplayOptions options;
    options.max_records = 4;
    log.open(file.path(), options);
    CHECK(log.record_count() == 4);
    CHECK(log.truncated());
}

TEST_CASE("ReplayLog numbers matches by first appearance and finds a decision's match")
{
    TempLog file(synthetic_log(3, 4));
    ReplayLog log;
    log.open(file.path());
    REQUIRE(log.match_count() == 3);
    CHECK(log.match_start(1) == 4);
    CHECK(log.match_of(0) == 0);
    CHECK(log.match_of(3) == 0);
    CHECK(log.match_of(4) == 1);
    CHECK(log.match_of(11) == 2);
}

TEST_CASE("ReplayLog reports times only when the log carries them")
{
    TempLog timed(join_lines({ header_line(), small_decision(0, 0, 0), small_decision(0, 1, 1500) }));
    ReplayLog log;
    log.open(timed.path());
    CHECK(log.has_time());
    Pools pools;
    REQUIRE(log.read(1, pools.parsed));
    CHECK(pools.parsed.record.time_us == 1500);
}

TEST_CASE("ReplayLog indexes a 200k-line log in bounded time and reads across it")
{
    constexpr int32_t k_matches = 4000;
    constexpr int32_t k_per_match = 50;
    TempLog file(synthetic_log(k_matches, k_per_match));
    ReplayLog log;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    log.open(file.path());
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    REQUIRE(log.record_count() == static_cast<uint64_t>(k_matches) * k_per_match);
    CHECK(log.match_count() == static_cast<uint64_t>(k_matches));
    CHECK(seconds < 30.0);
    Pools pools;
    for (uint64_t index : { uint64_t{ 0 }, uint64_t{ 123456 }, uint64_t{ 199999 } })
    {
        REQUIRE(log.read(index, pools.parsed));
        CHECK(pools.parsed.record.decision_index == index % k_per_match);
        CHECK(pools.parsed.record.match_index == index / k_per_match);
    }
    CHECK(log.read_errors() == 0);
}

TEST_CASE("ReplayLog read is allocation free once open")
{
    if (!MemoryTracker::is_installed())
    {
        return;
    }
    TempLog file(join_lines({ header_line(), full_decision(0, 0), full_decision(0, 1) }));
    ReplayLog log;
    log.open(file.path());
    Pools pools;
    REQUIRE(log.read(0, pools.parsed));

    MemoryStats before = all_allocations();
    for (int32_t i = 0; i < 100; ++i)
    {
        CHECK(log.read(static_cast<uint64_t>(i % 2), pools.parsed));
    }
    CHECK(memory_delta(before, all_allocations()).allocation_count == 0);
}
