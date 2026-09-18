#pragma once

#include "Oryx/Benchmark/BenchmarkRunner.h"

namespace oryx
{

// Formats a BenchmarkRunner::Results (however it was produced - a headless
// BenchmarkRunner or a Layer-driven SimulationLayer batch) into a
// human-readable multi-line report: outcome (wins/draws/rewards),
// throughput (matches/decisions per second), and - if any OX_PROFILE_SCOPE
// call sites recorded samples - a per-scope timing breakdown. If profiling
// wasn't compiled into this build (Dist strips OX_ENABLE_PROFILING), that
// section is replaced with a note saying so, instead of being silently
// omitted.
std::string format_benchmark_report(const BenchmarkRunner::Results& results);

} // namespace oryx
