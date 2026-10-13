#include "doctest.h"

#include "Oryx.h"

#include "unit/MemoryTestSupport.h"

using namespace oryx;

#ifdef OX_ENABLE_LOG_CAPTURE

namespace
{

struct CleanRing
{
    CleanRing() { log_capture::clear(); }
    ~CleanRing() { log_capture::clear(); }
};

std::vector<LogLine> snapshot_all()
{
    std::vector<LogLine> lines(k_log_capacity);
    lines.resize(log_capture::snapshot(lines.data(), k_log_capacity));
    return lines;
}

std::string text_of(const LogLine& line)
{
    return std::string(line.text, line.length);
}

} // namespace

TEST_CASE("logcapture: lines come back oldest first with their level and source")
{
    CleanRing ring;
    log_capture::record(2, LogSource::App, "first");
    log_capture::record(4, LogSource::Core, "second");

    const std::vector<LogLine> lines = snapshot_all();
    REQUIRE(lines.size() == 2);
    CHECK(text_of(lines[0]) == "first");
    CHECK(lines[0].level == 2);
    CHECK(lines[0].source == LogSource::App);
    CHECK(text_of(lines[1]) == "second");
    CHECK(lines[1].sequence == lines[0].sequence + 1);
    CHECK(log_capture::sequence() == 2);
}

TEST_CASE("logcapture: the loggers feed the ring")
{
    CleanRing ring;
    OX_INFO("from the app logger {}", 7);
    const std::vector<LogLine> lines = snapshot_all();
    REQUIRE_FALSE(lines.empty());
    CHECK(text_of(lines.back()) == "from the app logger 7");
    CHECK(lines.back().source == LogSource::App);
}

TEST_CASE("logcapture: a full ring overwrites the oldest line and counts it as dropped")
{
    CleanRing ring;
    for (uint32_t i = 0; i < k_log_capacity + 5; ++i)
    {
        log_capture::record(2, LogSource::App, std::to_string(i));
    }
    const std::vector<LogLine> lines = snapshot_all();
    REQUIRE(lines.size() == k_log_capacity);
    CHECK(text_of(lines.front()) == "5");
    CHECK(text_of(lines.back()) == std::to_string(k_log_capacity + 4));
    CHECK(log_capture::dropped() == 5);
    CHECK(log_capture::sequence() == k_log_capacity + 5);
}

TEST_CASE("logcapture: a snapshot smaller than the ring keeps the newest lines")
{
    CleanRing ring;
    for (uint32_t i = 0; i < 10; ++i)
    {
        log_capture::record(2, LogSource::App, std::to_string(i));
    }
    LogLine out[3];
    CHECK(log_capture::snapshot(out, 3) == 3);
    CHECK(text_of(out[0]) == "7");
    CHECK(text_of(out[2]) == "9");
}

TEST_CASE("logcapture: a long line ends in an ellipsis at a character boundary")
{
    CleanRing ring;
    std::string text(k_log_text_capacity - 4, 'a');
    text += "\xC3\xA9\xC3\xA9\xC3\xA9";
    log_capture::record(2, LogSource::App, text);
    const std::vector<LogLine> lines = snapshot_all();
    REQUIRE(lines.size() == 1);
    const std::string kept = text_of(lines[0]);
    CHECK(kept.size() <= k_log_text_capacity);
    CHECK(kept.ends_with("\xE2\x80\xA6"));
    CHECK(kept.size() == k_log_text_capacity - 1);
}

TEST_CASE("logcapture: clear empties the ring and restarts the sequence")
{
    log_capture::record(2, LogSource::App, "x");
    log_capture::clear();
    CHECK(snapshot_all().empty());
    CHECK(log_capture::sequence() == 0);
    CHECK(log_capture::dropped() == 0);
}

TEST_CASE("logcapture: recording a line allocates nothing")
{
    CleanRing ring;
    log_capture::record(2, LogSource::App, "warm up");
    const MemoryStats before = test::all_allocations();
    for (uint32_t i = 0; i < 100; ++i)
    {
        log_capture::record(2, LogSource::App, "a line of ordinary length");
    }
    const MemoryStats after = test::all_allocations();
    CHECK(after.allocation_count == before.allocation_count);
}

TEST_CASE("logcapture: two threads logging while a third reads never tear a line or lose a count")
{
    CleanRing ring;
    constexpr uint32_t k_per_thread = 5000;
    std::atomic<bool> done{ false };
    std::atomic<uint32_t> bad{ 0 };

    std::thread reader(
        [&]
        {
            std::vector<LogLine> lines(k_log_capacity);
            while (!done.load())
            {
                const uint32_t count = log_capture::snapshot(lines.data(), k_log_capacity);
                for (uint32_t i = 0; i < count; ++i)
                {
                    const LogLine& line = lines[i];
                    const bool whole = line.length == 4 && std::memcmp(line.text, line.source == LogSource::Core ? "core" : "app_", 4) == 0;
                    if (!whole || (i > 0 && line.sequence != lines[i - 1].sequence + 1))
                    {
                        bad.fetch_add(1);
                    }
                }
            }
        });
    std::thread writer_a([] { for (uint32_t i = 0; i < k_per_thread; ++i) { log_capture::record(2, LogSource::Core, "core"); } });
    std::thread writer_b([] { for (uint32_t i = 0; i < k_per_thread; ++i) { log_capture::record(2, LogSource::App, "app_"); } });
    writer_a.join();
    writer_b.join();
    done.store(true);
    reader.join();

    CHECK(bad.load() == 0);
    CHECK(log_capture::sequence() == 2 * k_per_thread);
    CHECK(snapshot_all().size() + log_capture::dropped() == 2 * k_per_thread);
}

#else

TEST_CASE("logcapture: a build without it reports unavailable and records nothing")
{
    CHECK_FALSE(log_capture::available());
    log_capture::record(2, LogSource::App, "x");
    CHECK(log_capture::sequence() == 0);
}

#endif
