#include "BenchmarkReport.h"

#include "Oryx/Debug/Instrumentation.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace oryx
{

namespace
{

void write_outcome_quality(std::ostringstream& out, const BatchResult& outcome)
{
    out << "--- Benchmark report ---\n";
    out << "outcome quality:\n";
    out << "  matches: " << outcome.matches << ", draws: " << outcome.draws << "\n";
    for (size_t player = 0; player < outcome.wins.size(); ++player)
    {
        out << "  player " << player << ": " << outcome.wins[player] << " win(s), "
            << "avg reward " << (outcome.rewards[static_cast<PlayerId>(player)] / static_cast<double>(std::max(outcome.matches, 1))) << "\n";
    }
}

void write_performance(std::ostringstream& out, const BenchmarkRunner::Results& results)
{
    out << "performance:\n";
    out << "  decisions: " << results.outcome.decisions << "\n";
    out << "  elapsed: " << results.elapsed_seconds << "s, "
        << matches_per_second(results) << " matches/sec, "
        << decisions_per_second(results) << " decisions/sec\n";
}

void write_memory(std::ostringstream& out, const MemoryBenchmarkRunner::MemoryResults& results)
{
    const MemoryStats& memory = results.memory;
    double matches = static_cast<double>(std::max(results.outcome.matches, 1));
    double decisions = static_cast<double>(std::max<int64_t>(results.outcome.decisions, 1));

    out << "memory:\n";
    out << "  allocations: " << memory.allocation_count << " (" << (static_cast<double>(memory.allocation_count) / matches) << "/match, "
        << (static_cast<double>(memory.allocation_count) / decisions) << "/decision)\n";
    out << "  bytes allocated: " << memory.bytes_allocated << ", freed: " << memory.bytes_freed << "\n";
    out << "  peak live above baseline: " << memory.peak_live_bytes << " bytes, net live at end: " << memory.live_bytes << " bytes\n";
}

void write_profile(std::ostringstream& out)
{
    if (!Instrumentation::is_enabled())
    {
        out << "(profiling not compiled into this build - see OX_ENABLE_PROFILING)\n";
        return;
    }

    const std::unordered_map<std::string_view, ProfileSample>& profile = Instrumentation::results();
    if (profile.empty())
    {
        out << "profile: (no instrumented scopes ran)\n";
        return;
    }

    std::vector<std::string_view> names;
    names.reserve(profile.size());
    for (const auto& [name, sample] : profile)
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());

    out << std::fixed << std::setprecision(3);
    out << "profile:\n";
    for (std::string_view name : names)
    {
        const ProfileSample& sample = profile.at(name);
        double calls = static_cast<double>(sample.call_count);
        double avg_ns = (sample.total_milliseconds * 1'000'000.0) / calls;
        out << "  " << name << ": " << sample.call_count << " call(s), "
            << sample.total_milliseconds << "ms total, "
            << avg_ns << "ns/call avg, "
            << (sample.min_milliseconds * 1'000'000.0) << "ns/call min, "
            << (sample.max_milliseconds * 1'000'000.0) << "ns/call max";
        if (Instrumentation::tracks_memory())
        {
            out << ", " << (static_cast<double>(sample.allocation_count) / calls) << " allocs/call, "
                << (static_cast<double>(sample.bytes_allocated) / calls) << " bytes/call";
        }
        out << "\n";
    }
}

} // namespace

std::string format_benchmark_report(const BenchmarkRunner::Results& results)
{
    std::ostringstream out;
    write_outcome_quality(out, results.outcome);
    write_performance(out, results);
    write_profile(out);
    return out.str();
}

std::string format_benchmark_report(const MemoryBenchmarkRunner::MemoryResults& results)
{
    std::ostringstream out;
    write_outcome_quality(out, results.outcome);
    write_performance(out, results);
    write_memory(out, results);
    write_profile(out);
    return out.str();
}

} // namespace oryx
