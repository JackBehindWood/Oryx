#include "doctest.h"

#include "unit/Game/DummyGame.h"

#include "Oryx/Benchmark/BenchmarkReport.h"
#include "Oryx/Benchmark/BenchmarkRunner.h"
#include "Oryx/Benchmark/MemoryBenchmarkRunner.h"
#include "Oryx/Benchmark/Timer.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("Timer measures a non-negative interval between start and stop")
{
    Timer timer;
    timer.start();
    timer.stop();

    CHECK(timer.elapsed_seconds() >= 0.0);
}

TEST_CASE("Throughput helpers divide by elapsed time and return 0 instead of infinity when no time elapsed")
{
    BenchmarkRunner::Results results;
    results.outcome.matches = 10;
    results.outcome.decisions = 50;

    results.elapsed_seconds = 2.0;
    CHECK(matches_per_second(results) == doctest::Approx(5.0));
    CHECK(decisions_per_second(results) == doctest::Approx(25.0));

    results.elapsed_seconds = 0.0;
    CHECK(matches_per_second(results) == 0.0);
    CHECK(decisions_per_second(results) == 0.0);
}

TEST_CASE("BenchmarkRunner::run plays the requested matches and reports elapsed time")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    BenchmarkRunner runner(game, { &strategy_a, &strategy_b });

    BenchmarkRunner::Results results = runner.run(5);

    CHECK(results.outcome.matches == 5);
    CHECK(results.outcome.decisions > 0);
    CHECK(results.elapsed_seconds >= 0.0);
}

TEST_CASE("MemoryBenchmarkRunner::run adds a memory delta to the base runner's results")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    MemoryBenchmarkRunner runner(game, { &strategy_a, &strategy_b });

    MemoryBenchmarkRunner::MemoryResults results = runner.run(5);
    const BenchmarkRunner::Results& base = results;

    CHECK(base.outcome.matches == 5);
    CHECK(base.elapsed_seconds >= 0.0);
    CHECK(results.memory.allocation_count >= 5);
    CHECK(results.memory.bytes_allocated > 0);
}

TEST_CASE("format_benchmark_report(Results) has outcome and throughput but no memory section")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    BenchmarkRunner runner(game, { &strategy_a, &strategy_b });

    std::string report = format_benchmark_report(runner.run(3));

    CHECK(report.find("matches: 3") != std::string::npos);
    CHECK(report.find("matches/sec") != std::string::npos);
    CHECK(report.find("memory:") == std::string::npos);
}

TEST_CASE("format_benchmark_report keeps outcome quality and performance in separate sections")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    BenchmarkRunner runner(game, { &strategy_a, &strategy_b });

    std::string report = format_benchmark_report(runner.run(3));

    size_t quality = report.find("outcome quality:");
    size_t performance = report.find("performance:");
    REQUIRE(quality != std::string::npos);
    REQUIRE(performance != std::string::npos);
    CHECK(quality < performance);
    CHECK(report.find("win(s)") > quality);
    CHECK(report.find("win(s)") < performance);
    CHECK(report.find("matches/sec") > performance);
    CHECK(report.find("decisions:") > performance);
}

TEST_CASE("format_benchmark_report(MemoryResults) adds the memory section in every build configuration")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    MemoryBenchmarkRunner runner(game, { &strategy_a, &strategy_b });

    std::string report = format_benchmark_report(runner.run(3));

    CHECK(report.find("matches: 3") != std::string::npos);
    CHECK(report.find("memory:") != std::string::npos);
    CHECK(report.find("allocations:") != std::string::npos);
}

#ifdef OX_ENABLE_PROFILING
TEST_CASE("format_benchmark_report lists profiled scopes")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    BenchmarkRunner runner(game, { &strategy_a, &strategy_b });

    std::string report = format_benchmark_report(runner.run(3));

    CHECK(report.find("profile:") != std::string::npos);
    CHECK(report.find("DummyState::legal_actions") != std::string::npos);
}
#endif

#ifdef OX_ENABLE_MEMORY_TRACKING
TEST_CASE("format_benchmark_report lists allocs/call and bytes/call per profiled scope")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;
    BenchmarkRunner runner(game, { &strategy_a, &strategy_b });

    std::string report = format_benchmark_report(runner.run(3));

    CHECK(report.find("allocs/call") != std::string::npos);
    CHECK(report.find("bytes/call") != std::string::npos);
}
#endif
