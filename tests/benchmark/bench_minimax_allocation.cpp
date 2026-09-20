#include "doctest.h"

#include "unit/Game/DummyGame.h"

#include <chrono>

using namespace oryx;
using namespace oryx::test;

namespace
{

// Pile size tuned so DummyGame's full (unmemoized) exhaustive search tree is
// comparable in order of magnitude to Tic-Tac-Toe's ~5x10^5 nodes.
constexpr uint32_t kPileSize = 20;

constexpr int32_t kAllocIterations = 2'000'000;

using Clock = std::chrono::steady_clock;

double milliseconds_since(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

} // namespace

// Excluded from the default `build test` run (see build_system/commands/test.py)
// - these measure wall-clock cost, not correctness, and are slow/noisy in CI.
// Run explicitly via `build test benchmark`.
TEST_SUITE("benchmark")
{

TEST_CASE("Benchmark: MinimaxStrategy exhaustive search allocation cost (DummyGame)")
{
    DummyGame game(kPileSize);
    MinimaxStrategy strategy;
    UniquePtr<IState> state = game.new_initial_state();
    Context context(*state);

    Instrumentation::reset();
    MemoryTracker::reset_peak();
    MemoryStats memory_before = MemoryTracker::snapshot();

    Clock::time_point search_start = Clock::now();
    ActionId action = strategy.decide(context);
    double search_ms = milliseconds_since(search_start);

    MemoryStats memory = memory_delta(memory_before, MemoryTracker::snapshot());

    CHECK(is_valid(action));

    const std::unordered_map<std::string_view, ProfileSample>& results = Instrumentation::results();
    std::unordered_map<std::string_view, ProfileSample>::const_iterator it = results.find("DummyState::legal_actions");
    REQUIRE(it != results.end());

    double legal_actions_ms = it->second.total_milliseconds;
    int64_t call_count = it->second.call_count;

    MESSAGE("legal_actions() calls: ", call_count);
    MESSAGE("legal_actions() total ms: ", legal_actions_ms);
    MESSAGE("decide() total ms: ", search_ms);
    MESSAGE("legal_actions() share of decide(): ", (legal_actions_ms / search_ms) * 100.0, "%");
    MESSAGE("ns/call (legal_actions, incl. timer overhead): ", (legal_actions_ms * 1'000'000.0) / static_cast<double>(call_count));

    MESSAGE("decide() allocations: ", memory.allocation_count, ", bytes: ", memory.bytes_allocated, ", peak live: ", memory.peak_live_bytes);
    MESSAGE("legal_actions() allocations: ", it->second.allocation_count, " over ", call_count, " calls");
}

TEST_CASE("Benchmark: heap vector vs. fixed-size array for a legal_actions()-shaped result")
{
    // Isolates pure allocator cost from search logic: builds/destroys a
    // <=3-element result kAllocIterations times via std::vector, a fixed
    // stack array, and ActionList (the real legal_actions() return type).
    volatile size_t sink = 0;

    MemoryStats vector_before = MemoryTracker::snapshot();
    Clock::time_point vector_start = Clock::now();
    for (int32_t i = 0; i < kAllocIterations; ++i)
    {
        std::vector<ActionId> actions;
        actions.push_back(1);
        actions.push_back(2);
        actions.push_back(3);
        sink += actions.size();
    }
    double vector_ms = milliseconds_since(vector_start);
    MemoryStats vector_memory = memory_delta(vector_before, MemoryTracker::snapshot());

    Clock::time_point array_start = Clock::now();
    for (int32_t i = 0; i < kAllocIterations; ++i)
    {
        ActionId actions[3] = { 1, 2, 3 };
        sink += actions[0] + actions[1] + actions[2];
    }
    double array_ms = milliseconds_since(array_start);

    MemoryStats action_list_before = MemoryTracker::snapshot();
    Clock::time_point action_list_start = Clock::now();
    for (int32_t i = 0; i < kAllocIterations; ++i)
    {
        ActionList actions;
        actions.push_back(1);
        actions.push_back(2);
        actions.push_back(3);
        sink += actions.size();
    }
    double action_list_ms = milliseconds_since(action_list_start);
    MemoryStats action_list_memory = memory_delta(action_list_before, MemoryTracker::snapshot());

    MESSAGE("std::vector<ActionId> (3 elements) x", kAllocIterations, ": ", vector_ms, " ms");
    MESSAGE("fixed ActionId[3] x", kAllocIterations, ": ", array_ms, " ms");
    MESSAGE("ActionList (3 elements) x", kAllocIterations, ": ", action_list_ms, " ms");
    MESSAGE("ns/call heap vector: ", (vector_ms * 1'000'000.0) / kAllocIterations);
    MESSAGE("ns/call fixed array: ", (array_ms * 1'000'000.0) / kAllocIterations);
    MESSAGE("ns/call ActionList: ", (action_list_ms * 1'000'000.0) / kAllocIterations);

    MESSAGE("heap allocations, std::vector: ", vector_memory.allocation_count);
    MESSAGE("heap allocations, ActionList: ", action_list_memory.allocation_count);

    CHECK(sink > 0);
}

} // TEST_SUITE("benchmark")
