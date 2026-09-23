#pragma once

#include "Oryx/Benchmark/BenchmarkRunner.h"
#include "Oryx/Memory/MemoryStats.h"

namespace oryx
{

class MemoryBenchmarkRunner : public BenchmarkRunner
{
public:
    using BenchmarkRunner::BenchmarkRunner;

    struct MemoryResults : public BenchmarkRunner::Results
    {
        MemoryStats memory;
    };

    MemoryResults run(int32_t match_count);
};

} // namespace oryx
