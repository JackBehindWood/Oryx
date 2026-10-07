#include "doctest.h"

#include "Oryx.h"
#include "unit/MemoryTestSupport.h"

using namespace oryx;

namespace
{

struct FirstContext : ImContext
{
};

struct SecondContext : ImContext
{
};

} // namespace

TEST_CASE("ImId: stable, scoped by parent and never none")
{
    CHECK(make_im_id("ok") == make_im_id("ok"));
    CHECK(make_im_id("ok") != make_im_id("no"));
    const ImId panel = make_im_id("panel");
    CHECK(make_im_id("ok", panel) != make_im_id("ok"));
    CHECK(make_im_id("ok", panel) == make_im_id("ok", panel));
    CHECK(make_im_index_id(3, panel) == make_im_index_id(3, panel));
    CHECK(make_im_index_id(3, panel) != make_im_index_id(4, panel));
    CHECK(make_im_index_id(3, panel) != make_im_index_id(3));
    CHECK(is_valid(make_im_id("")));
    CHECK_FALSE(is_valid(ImId{}));
}

TEST_CASE("StateTable: entries persist while used and are collected when not")
{
    StateTable<int32_t> table;
    const ImId a = make_im_id("a");
    const ImId b = make_im_id("b");
    table.get(a, 1) = 10;
    table.get(b, 1) = 20;
    table.collect(1);
    CHECK(table.size() == 2);
    table.get(a, 2);
    table.collect(2);
    CHECK(table.size() == 1);
    REQUIRE(table.find(a) != nullptr);
    CHECK(*table.find(a) == 10);
    CHECK(table.find(b) == nullptr);
    CHECK(table.get(b, 3) == 0);
}

TEST_CASE("StateTable: a warm table allocates nothing")
{
    StateTable<int32_t> table;
    for (uint32_t i = 0; i < 50; ++i)
    {
        table.get(make_im_index_id(i), 1);
    }
    table.collect(2);
    for (uint32_t i = 0; i < 50; ++i)
    {
        table.get(make_im_index_id(i), 2);
    }
    MemoryStats before = test::all_allocations();
    table.collect(3);
    for (uint32_t i = 0; i < 50; ++i)
    {
        table.get(make_im_index_id(i), 3);
    }
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

TEST_CASE("FrameArena: storage, formatting and reuse after reset")
{
    FrameArena arena(64);
    const std::string_view stored = arena.store("hello");
    CHECK(stored == "hello");
    CHECK(arena.format("%d/%s", 7, "x") == "7/x");
    const std::span<uint64_t> numbers = arena.allocate_array<uint64_t>(4);
    CHECK(reinterpret_cast<uintptr_t>(numbers.data()) % alignof(uint64_t) == 0);
    CHECK(numbers.size() == 4);
    CHECK(stored == "hello");
    const std::string_view big = arena.store(std::string(500, 'z'));
    CHECK(big.size() == 500);
    CHECK(arena.capacity() >= 500);
    arena.reset();
    CHECK(arena.used() == 0);
    CHECK_THROWS_AS(arena.allocate(4, 3), Error);
}

TEST_CASE("FrameArena: a warm arena allocates nothing")
{
    FrameArena arena(256);
    auto frame = [&arena]()
    {
        arena.reset();
        for (uint32_t i = 0; i < 40; ++i)
        {
            std::ignore = arena.format("row %u", i);
        }
    };
    frame();
    frame();
    MemoryStats before = test::all_allocations();
    frame();
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

TEST_CASE("ImTheme: variants resolve by name, fall back to the base and replace")
{
    ImTheme theme;
    ImStyle primary;
    primary.accent = { 1.0f, 0.0f, 0.0f, 1.0f };
    add_style_variant(theme, "primary", primary);
    CHECK(style_for(theme, "primary").accent == primary.accent);
    CHECK(style_for(theme, make_im_id("primary")).accent == primary.accent);
    CHECK(style_for(theme, "missing").accent == theme.base.accent);
    primary.accent = { 0.0f, 1.0f, 0.0f, 1.0f };
    add_style_variant(theme, "primary", primary);
    CHECK(theme.variant_count == 1);
    CHECK(style_for(theme, "primary").accent == primary.accent);
}

TEST_CASE("ImTheme: running out of variant slots throws")
{
    ImTheme theme;
    for (uint32_t i = 0; i < k_max_style_variants; ++i)
    {
        add_style_variant(theme, std::to_string(i), {});
    }
    CHECK_THROWS_AS(add_style_variant(theme, "one too many", {}), Error);
}

TEST_CASE("ActiveContext: scopes swap and restore, also on a throw")
{
    FirstContext outer;
    FirstContext inner;
    CHECK(ActiveContext<FirstContext>::get() == nullptr);
    CHECK_THROWS_AS(ActiveContext<FirstContext>::require(), Error);
    {
        ContextScope<FirstContext> a(outer);
        CHECK(ActiveContext<FirstContext>::get() == &outer);
        try
        {
            ContextScope<FirstContext> b(inner);
            CHECK(&ActiveContext<FirstContext>::require() == &inner);
            throw Error("boom");
        }
        catch (const Error&)
        {
        }
        CHECK(ActiveContext<FirstContext>::get() == &outer);
    }
    CHECK(ActiveContext<FirstContext>::get() == nullptr);
}

TEST_CASE("ActiveContext: each context type has its own slot")
{
    FirstContext first;
    SecondContext second;
    ContextScope<FirstContext> scope(first);
    CHECK(ActiveContext<SecondContext>::get() == nullptr);
    ContextScope<SecondContext> other(second);
    CHECK(ActiveContext<FirstContext>::get() == &first);
    CHECK(ActiveContext<SecondContext>::get() == &second);
}
