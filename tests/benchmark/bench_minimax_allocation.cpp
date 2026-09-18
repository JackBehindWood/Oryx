#include "doctest.h"

#include "unit/Game/DummyGame.h"

#include <chrono>

using namespace oryx;
using namespace oryx::test;

namespace
{

// Pile size tuned so DummyGame's full (unmemoized) exhaustive search tree is
// comparable in order of magnitude to Tic-Tac-Toe's ~5x10^5 nodes - see
// DESIGN.md §12/§19 for what this benchmark is evidence for.
constexpr uint32_t kPileSize = 20;

constexpr int32_t kAllocIterations = 2'000'000;

} // namespace

// Excluded from the default `build test` run (see build_system/commands/test.py)
// - these measure wall-clock cost, not correctness, and are slow/noisy in CI.
// Run explicitly via `build test benchmark`.
TEST_SUITE("benchmark")
{

TEST_CASE("Benchmark: MinimaxStrategy exhaustive search allocation cost (DummyGame)")
{
    Instrumentation::reset();

    DummyGame game(kPileSize);
    MinimaxStrategy strategy;
    UniquePtr<IState> state = game.new_initial_state();
    Context context(*state);

    auto search_start = std::chrono::high_resolution_clock::now();
    ActionId action = strategy.decide(context);
    auto search_end = std::chrono::high_resolution_clock::now();
    double search_ms = std::chrono::duration<double, std::milli>(search_end - search_start).count();

    CHECK(is_valid(action));

    auto results = Instrumentation::results();
    auto it = results.find("DummyState::legal_actions");
    REQUIRE(it != results.end());

    double legal_actions_ms = it->second.total_milliseconds;
    int64_t call_count = it->second.call_count;

    MESSAGE("legal_actions() calls: ", call_count);
    MESSAGE("legal_actions() total ms: ", legal_actions_ms);
    MESSAGE("decide() total ms: ", search_ms);
    MESSAGE("legal_actions() share of decide(): ", (legal_actions_ms / search_ms) * 100.0, "%");
    MESSAGE("ns/call (legal_actions, incl. timer overhead): ", (legal_actions_ms * 1'000'000.0) / static_cast<double>(call_count));
}

TEST_CASE("Benchmark: heap vector vs. fixed-size array for a legal_actions()-shaped result")
{
    // Isolates pure allocator cost from search logic: builds/destroys a
    // <=3-element result kAllocIterations times, once via std::vector
    // (today's IState::legal_actions() return type) and once via a
    // fixed-size stack array of the same shape - the concrete "what would a
    // small-vector optimization actually save" number.
    volatile size_t sink = 0;

    auto vector_start = std::chrono::high_resolution_clock::now();
    for (int32_t i = 0; i < kAllocIterations; ++i)
    {
        std::vector<ActionId> actions;
        actions.push_back(1);
        actions.push_back(2);
        actions.push_back(3);
        sink += actions.size();
    }
    auto vector_end = std::chrono::high_resolution_clock::now();
    double vector_ms = std::chrono::duration<double, std::milli>(vector_end - vector_start).count();

    auto array_start = std::chrono::high_resolution_clock::now();
    for (int32_t i = 0; i < kAllocIterations; ++i)
    {
        ActionId actions[3] = { 1, 2, 3 };
        sink += actions[0] + actions[1] + actions[2];
    }
    auto array_end = std::chrono::high_resolution_clock::now();
    double array_ms = std::chrono::duration<double, std::milli>(array_end - array_start).count();

    // ActionList at the same shape - the real legal_actions() replacement.
    auto action_list_start = std::chrono::high_resolution_clock::now();
    for (int32_t i = 0; i < kAllocIterations; ++i)
    {
        ActionList actions;
        actions.push_back(1);
        actions.push_back(2);
        actions.push_back(3);
        sink += actions.size();
    }
    auto action_list_end = std::chrono::high_resolution_clock::now();
    double action_list_ms = std::chrono::duration<double, std::milli>(action_list_end - action_list_start).count();

    MESSAGE("std::vector<ActionId> (3 elements) x", kAllocIterations, ": ", vector_ms, " ms");
    MESSAGE("fixed ActionId[3] x", kAllocIterations, ": ", array_ms, " ms");
    MESSAGE("ActionList (3 elements) x", kAllocIterations, ": ", action_list_ms, " ms");
    MESSAGE("ns/call heap vector: ", (vector_ms * 1'000'000.0) / kAllocIterations);
    MESSAGE("ns/call fixed array: ", (array_ms * 1'000'000.0) / kAllocIterations);
    MESSAGE("ns/call ActionList: ", (action_list_ms * 1'000'000.0) / kAllocIterations);

    CHECK(sink > 0);
}

} // TEST_SUITE("benchmark")
