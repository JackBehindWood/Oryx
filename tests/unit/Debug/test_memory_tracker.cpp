#include "doctest.h"

#include "unit/Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

void* volatile g_escape = nullptr;

// Stops the optimizer from eliding a new/delete pair (permitted since C++14) that the test needs to observe.
void escape(void* pointer)
{
    g_escape = pointer;
}

struct alignas(64) OverAligned
{
    char data[64];
};

} // namespace

TEST_CASE("MemoryTracker counts a new/delete pair and returns live bytes to baseline")
{
    MemoryStats before = MemoryTracker::snapshot();
    int32_t* value = new int32_t(7);
    escape(value);
    MemoryStats during = memory_delta(before, MemoryTracker::snapshot());
    delete value;
    MemoryStats after = memory_delta(before, MemoryTracker::snapshot());

    CHECK(during.allocation_count == 1);
    CHECK(during.bytes_allocated == sizeof(int32_t));
    CHECK(during.live_bytes == static_cast<int64_t>(sizeof(int32_t)));
    CHECK(after.deallocation_count == 1);
    CHECK(after.bytes_freed == sizeof(int32_t));
    CHECK(after.live_bytes == 0);
}

TEST_CASE("MemoryTracker counts array new/delete by requested bytes")
{
    MemoryStats before = MemoryTracker::snapshot();
    int32_t* values = new int32_t[10];
    escape(values);
    delete[] values;
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(delta.allocation_count == 1);
    CHECK(delta.bytes_allocated == 10 * sizeof(int32_t));
    CHECK(delta.live_bytes == 0);
}

TEST_CASE("MemoryTracker honours over-aligned allocations")
{
    MemoryStats before = MemoryTracker::snapshot();
    OverAligned* value = new OverAligned;
    escape(value);
    bool aligned = reinterpret_cast<uintptr_t>(value) % alignof(OverAligned) == 0;
    delete value;
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(aligned);
    CHECK(delta.allocation_count == 1);
    CHECK(delta.bytes_allocated == sizeof(OverAligned));
    CHECK(delta.live_bytes == 0);
}

TEST_CASE("MemoryTracker reports peak growth above the starting live bytes after reset_peak")
{
    constexpr size_t kBlockBytes = 4096;

    MemoryTracker::reset_peak();
    MemoryStats before = MemoryTracker::snapshot();
    char* block = new char[kBlockBytes];
    escape(block);
    delete[] block;
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(delta.peak_live_bytes >= static_cast<int64_t>(kBlockBytes));
    CHECK(delta.live_bytes == 0);
}

TEST_CASE("Allocation budget: ActionList within its inline capacity never touches the heap")
{
    MemoryStats before = MemoryTracker::snapshot();
    ActionList actions;
    for (ActionId action = 0; action < kActionListInlineCapacity; ++action)
    {
        actions.push_back(action);
    }
    MemoryStats inline_delta = memory_delta(before, MemoryTracker::snapshot());

    actions.push_back(kActionListInlineCapacity);
    MemoryStats spilled_delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(inline_delta.allocation_count == 0);
    CHECK(spilled_delta.allocation_count == 1);
}

TEST_CASE("Allocation budget: two-player Rewards construct and copy never touch the heap")
{
    MemoryStats before = MemoryTracker::snapshot();
    Rewards<double> rewards(2);
    rewards[0] = 1.0;
    Rewards<double> copy = rewards;
    escape(&copy);
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(delta.allocation_count == 0);
}

TEST_CASE("Allocation budget: providing capabilities to a Context never touches the heap")
{
    struct NoFeatures : IActionFeatures
    {
        SmallVector<int32_t, 2> decode(ActionId) const override { return { 0, 0 }; }
    } features;

    DummyGame game(3);
    UniquePtr<IState> state = game.new_initial_state();

    MemoryStats before = MemoryTracker::snapshot();
    Context context(*state);
    context.provide<IActionFeatures>(&features);
    Context moved = std::move(context);
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(moved.has<IActionFeatures>());
    CHECK(delta.allocation_count == 0);
}

TEST_CASE("Allocation budget: a whole Match allocates only its initial state")
{
    DummyGame game(10);
    DummyGreedyStrategy strategy_a;
    DummyGreedyStrategy strategy_b;

    Match(game, { &strategy_a, &strategy_b }).play();

    MemoryStats before = MemoryTracker::snapshot();
    Match match(game, { &strategy_a, &strategy_b });
    match.play();
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(delta.allocation_count == 1);
}

TEST_CASE("Allocation budget: MinimaxStrategy::decide performs no heap allocation once warm")
{
    DummyGame game(12);
    MinimaxStrategy strategy;
    UniquePtr<IState> state = game.new_initial_state();
    Context context(*state);

    strategy.decide(context);

    MemoryStats before = MemoryTracker::snapshot();
    ActionId action = strategy.decide(context);
    MemoryStats delta = memory_delta(before, MemoryTracker::snapshot());

    CHECK(is_valid(action));
    CHECK(delta.allocation_count == 0);
}
