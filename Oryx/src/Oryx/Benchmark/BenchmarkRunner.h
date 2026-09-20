#pragma once

#include "Oryx/Simulation/BatchRunner.h"

namespace oryx
{

// Headless strategy-vs-strategy benchmark: no Application/Layer needed (tests, tooling, future Python bindings).
class BenchmarkRunner : public BatchRunner
{
public:
    using BatchRunner::BatchRunner;

    struct Results
    {
        BatchResult outcome;
        double elapsed_seconds = 0.0;
    };

    Results run(int32_t match_count);
};

[[nodiscard]] double matches_per_second(const BenchmarkRunner::Results& results);
[[nodiscard]] double decisions_per_second(const BenchmarkRunner::Results& results);

} // namespace oryx
