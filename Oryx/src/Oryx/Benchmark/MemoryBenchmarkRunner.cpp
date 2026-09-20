#include "MemoryBenchmarkRunner.h"

#include "Oryx/Debug/Instrumentation.h"

namespace oryx
{

MemoryBenchmarkRunner::MemoryResults MemoryBenchmarkRunner::run(int32_t match_count)
{
    // Clears the registry outside the measured window; BenchmarkRunner::run's own reset is then a no-op.
    Instrumentation::reset();
    MemoryStats memory_before = MemoryTracker::begin_measurement();

    MemoryResults results;
    static_cast<BenchmarkRunner::Results&>(results) = BenchmarkRunner::run(match_count);

    results.memory = memory_delta(memory_before, MemoryTracker::snapshot());
    return results;
}

} // namespace oryx
