#include "BenchmarkRunner.h"

#include "Oryx/Benchmark/Timer.h"
#include "Oryx/Debug/Instrumentation.h"

namespace oryx
{

BenchmarkRunner::Results BenchmarkRunner::run(int32_t match_count)
{
    Instrumentation::reset();

    Timer timer;
    timer.start();
    BatchResult outcome = BatchRunner::run(match_count);
    timer.stop();

    return Results{ outcome, timer.elapsed_seconds() };
}

double matches_per_second(const BenchmarkRunner::Results& results)
{
    return results.elapsed_seconds > 0.0 ? static_cast<double>(results.outcome.matches) / results.elapsed_seconds : 0.0;
}

double decisions_per_second(const BenchmarkRunner::Results& results)
{
    return results.elapsed_seconds > 0.0 ? static_cast<double>(results.outcome.decisions) / results.elapsed_seconds : 0.0;
}

} // namespace oryx
