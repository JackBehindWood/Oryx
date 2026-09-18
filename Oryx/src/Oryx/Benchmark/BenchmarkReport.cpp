#include "BenchmarkReport.h"

#include "Oryx/Debug/Instrumentation.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <vector>

namespace oryx
{

std::string format_benchmark_report(const BenchmarkRunner::Results& results)
{
    const BatchResult& outcome = results.outcome;
    std::ostringstream out;

    out << "--- Benchmark report ---\n";
    out << "matches: " << outcome.matches << ", draws: " << outcome.draws
        << ", decisions: " << outcome.decisions << "\n";
    for (size_t player = 0; player < outcome.wins.size(); ++player)
    {
        out << "  player " << player << ": " << outcome.wins[player] << " win(s), "
            << "avg reward " << (outcome.rewards[static_cast<PlayerId>(player)] / static_cast<double>(std::max(outcome.matches, 1))) << "\n";
    }
    out << "elapsed: " << results.elapsed_seconds << "s, "
        << results.matches_per_second() << " matches/sec, "
        << results.decisions_per_second() << " decisions/sec\n";

    std::unordered_map<std::string, ProfileSample> profile = Instrumentation::results();
    if (profile.empty())
    {
        out << "(profiling not compiled into this build - see OX_ENABLE_PROFILING)\n";
        return out.str();
    }

    std::vector<std::string> names;
    names.reserve(profile.size());
    for (const auto& [name, sample] : profile)
    {
        names.push_back(name);
    }
    std::sort(names.begin(), names.end());

    out << std::fixed << std::setprecision(3);
    out << "profile:\n";
    for (const std::string& name : names)
    {
        const ProfileSample& sample = profile.at(name);
        double avg_ns = (sample.total_milliseconds * 1'000'000.0) / static_cast<double>(sample.call_count);
        out << "  " << name << ": " << sample.call_count << " call(s), "
            << sample.total_milliseconds << "ms total, "
            << avg_ns << "ns/call avg, "
            << (sample.min_milliseconds * 1'000'000.0) << "ns/call min, "
            << (sample.max_milliseconds * 1'000'000.0) << "ns/call max\n";
    }

    return out.str();
}

} // namespace oryx
