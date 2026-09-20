#pragma once

#include "Oryx/Benchmark/MemoryBenchmarkRunner.h"

namespace oryx
{

// Outcome, throughput and per-scope profile; a profile section stripped from this build (Dist) is replaced by a note.
std::string format_benchmark_report(const BenchmarkRunner::Results& results);

// Same as above with the run's memory section between throughput and profile.
std::string format_benchmark_report(const MemoryBenchmarkRunner::MemoryResults& results);

} // namespace oryx
