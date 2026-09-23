#include "doctest.h"

#include "Oryx/Memory/CountingAllocator.h"
#include "Oryx/Memory/HeapAllocator.h"

using namespace oryx;

namespace
{

int32_t g_destroyed = 0;

class Base
{
public:
    virtual ~Base() = default;
    int32_t base_value = 1;
};

class Other
{
public:
    virtual ~Other() = default;
    int64_t other_value = 2;
};

class Derived : public Base, public Other
{
public:
    explicit Derived(int32_t value)
        : derived_value(value)
    {
    }
    ~Derived() override { ++g_destroyed; }
    int32_t derived_value;
};

class Throws
{
public:
    Throws() { throw std::runtime_error("constructor failed"); }
};

class NonVirtual
{
public:
    ~NonVirtual() { ++g_destroyed; }
};

} // namespace

TEST_SUITE("memory")
{

TEST_CASE("UniquePtr and SharedPtr are two pointers wide")
{
    CHECK(sizeof(UniquePtr<int32_t>) == 2 * sizeof(void*));
    CHECK(sizeof(SharedPtr<int32_t>) == 2 * sizeof(void*));
}

TEST_CASE("allocate_unique uses one block and frees its exact size through a base pointer")
{
    g_destroyed = 0;
    CountingAllocator counting(heap_allocator());
    {
        UniquePtr<Other> other = allocate_unique<Derived>(counting, 7);
        CHECK(other->other_value == 2);
        CHECK(static_cast<Derived*>(other.get())->derived_value == 7);
        MemoryStats stats = counting.counters().snapshot();
        CHECK(stats.allocation_count == 1);
        CHECK(stats.bytes_allocated == detail::BlockLayout<Derived>::size);
    }
    MemoryStats stats = counting.counters().snapshot();
    CHECK(g_destroyed == 1);
    CHECK(stats.deallocation_count == 1);
    CHECK(stats.bytes_freed == stats.bytes_allocated);
}

TEST_CASE("the destroy thunk runs the concrete destructor even without a virtual one")
{
    g_destroyed = 0;
    {
        SharedPtr<void> erased = create_shared<NonVirtual>();
        CHECK(erased != nullptr);
    }
    CHECK(g_destroyed == 1);
}

TEST_CASE("a UniquePtr moves into a SharedPtr without a new allocation and dies with the last reference")
{
    g_destroyed = 0;
    CountingAllocator counting(heap_allocator());
    UniquePtr<Derived> unique = allocate_unique<Derived>(counting, 3);
    SharedPtr<Base> shared = std::move(unique);
    CHECK(unique == nullptr);
    CHECK(shared.use_count() == 1);

    SharedPtr<Base> copy = shared;
    SharedPtr<const Base> readonly = copy;
    CHECK(shared.use_count() == 3);
    CHECK(counting.counters().snapshot().allocation_count == 1);

    shared.reset();
    copy = nullptr;
    CHECK(g_destroyed == 0);
    CHECK(readonly->base_value == 1);
    readonly.reset();
    CHECK(g_destroyed == 1);
    CHECK(counting.counters().snapshot().live_bytes == 0);
}

TEST_CASE("create_shared of a const type and moves between pointers keep one owner")
{
    SharedPtr<const std::string> text = create_shared<const std::string>("oryx");
    SharedPtr<const std::string> moved = std::move(text);
    CHECK(text == nullptr);
    CHECK(*moved == "oryx");

    UniquePtr<Base> first = create_unique<Derived>(1);
    UniquePtr<Base> second;
    second = std::move(first);
    CHECK(first == nullptr);
    CHECK(second->base_value == 1);
}

TEST_CASE("a throwing constructor returns its block to the allocator")
{
    CountingAllocator counting(heap_allocator());
    CHECK_THROWS_AS(allocate_unique<Throws>(counting), std::runtime_error);
    CHECK_THROWS_AS(allocate_shared<Throws>(counting), std::runtime_error);
    MemoryStats stats = counting.counters().snapshot();
    CHECK(stats.allocation_count == 2);
    CHECK(stats.live_bytes == 0);
}

TEST_CASE("SharedPtr adopts an object made with new, as pybind11's holder machinery does")
{
    g_destroyed = 0;
    {
        SharedPtr<Base> adopted(new Derived(5));
        SharedPtr<Base> copy = adopted;
        CHECK(copy.use_count() == 2);
    }
    CHECK(g_destroyed == 1);
}

} // TEST_SUITE("memory")
