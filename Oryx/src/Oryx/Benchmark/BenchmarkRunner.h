#pragma once

#include "Oryx/Simulation/BatchRunner.h"

namespace oryx
{

// Headless strategy-vs-strategy benchmark: no Application/Layer needed, so
// it's usable directly (future Python bindings, tests, tooling). Publicly
// inherits BatchRunner rather than wrapping it - a BenchmarkRunner *is* a
// BatchRunner that also times itself, so it reuses the base constructor and
// game/strategy storage instead of duplicating them.
class BenchmarkRunner : public BatchRunner
{
public:
    using BatchRunner::BatchRunner;

    struct Results
    {
        BatchResult outcome;
        double elapsed_seconds = 0.0;

        [[nodiscard]] double matches_per_second() const { return static_cast<double>(outcome.matches) / elapsed_seconds; }
        [[nodiscard]] double decisions_per_second() const { return static_cast<double>(outcome.decisions) / elapsed_seconds; }
    };

    Results run(int32_t match_count);
};

} // namespace oryx
