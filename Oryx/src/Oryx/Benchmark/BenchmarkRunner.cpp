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

} // namespace oryx
