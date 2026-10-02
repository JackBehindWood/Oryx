#include "doctest.h"

#include "unit/MemoryTestSupport.h"

#include "Oryx/Memory/RefCounted.h"

using namespace oryx;

namespace
{

int32_t g_destroyed = 0;

class Resource : public RefCounted
{
public:
    explicit Resource(int32_t value = 0)
        : value(value)
    {
    }
    ~Resource() override { ++g_destroyed; }

    int32_t value;
};

class DerivedResource : public Resource
{
public:
    DerivedResource()
        : Resource(7)
    {
    }
};

class ThrowingResource : public RefCounted
{
public:
    ThrowingResource() { throw std::runtime_error("constructor failed"); }
};

class DeferringRetirer final : public IRetirer
{
public:
    void retire(RefCounted& object) noexcept override
    {
        ++retired;
        pending = &object;
    }

    void destroy_pending() noexcept
    {
        detail::destroy_now(*pending);
        pending = nullptr;
    }

    int32_t retired = 0;
    RefCounted* pending = nullptr;
};

} // namespace

TEST_SUITE("memory")
{

TEST_CASE("make_ref starts at one reference and destroys on the last release")
{
    g_destroyed = 0;
    {
        Ref<Resource> ref = make_ref<Resource>(3);
        CHECK(ref->value == 3);
        CHECK((*ref).value == 3);
        CHECK(ref->ref_count() == 1);
    }
    CHECK(g_destroyed == 1);
}

TEST_CASE("Ref copy, move and reset move the count")
{
    g_destroyed = 0;
    Ref<Resource> a = make_ref<Resource>();
    Ref<Resource> b = a;
    CHECK(a->ref_count() == 2);

    Ref<Resource> c = std::move(b);
    CHECK(b.get() == nullptr);
    CHECK(a->ref_count() == 2);

    Ref<Resource> d;
    d = a;
    CHECK(a->ref_count() == 3);
    d = std::move(c);
    CHECK(a->ref_count() == 2);

    d.reset();
    CHECK(a->ref_count() == 1);
    CHECK_FALSE(d);
    CHECK(g_destroyed == 0);

    a.reset();
    CHECK(g_destroyed == 1);
}

TEST_CASE("Ref converts derived to base and shares the count")
{
    g_destroyed = 0;
    Ref<DerivedResource> derived = make_ref<DerivedResource>();
    Ref<Resource> base = derived;
    CHECK(base->value == 7);
    CHECK(derived->ref_count() == 2);
    CHECK(base == derived);

    Ref<Resource> moved = std::move(derived);
    CHECK_FALSE(derived);
    CHECK(moved->ref_count() == 2);

    base.reset();
    moved.reset();
    CHECK(g_destroyed == 1);
}

TEST_CASE("Ref::from_raw takes a new reference")
{
    g_destroyed = 0;
    Ref<Resource> owner = make_ref<Resource>();
    Resource* raw = owner.get();

    Ref<Resource> second = Ref<Resource>::from_raw(raw);
    CHECK(owner->ref_count() == 2);
    CHECK(second == owner);

    CHECK_FALSE(Ref<Resource>::from_raw(nullptr));

    owner.reset();
    CHECK(g_destroyed == 0);
    second.reset();
    CHECK(g_destroyed == 1);
}

TEST_CASE("Ref swap exchanges the objects without touching counts")
{
    Ref<Resource> a = make_ref<Resource>(1);
    Ref<Resource> b = make_ref<Resource>(2);
    a.swap(b);
    CHECK(a->value == 2);
    CHECK(b->value == 1);
    CHECK(a->ref_count() == 1);
}

TEST_CASE("Default release returns the allocation to the census")
{
    MemoryStats before = test::all_allocations();
    {
        Ref<Resource> ref = make_ref<Resource>();
        Ref<Resource> copy = ref;
        CHECK(test::all_allocations().live_bytes > before.live_bytes);
    }
    MemoryStats after = test::all_allocations();
    CHECK(after.live_bytes == before.live_bytes);
    CHECK(after.allocation_count - before.allocation_count == after.deallocation_count - before.deallocation_count);
}

TEST_CASE("A retirer defers destruction until it destroys the object")
{
    g_destroyed = 0;
    DeferringRetirer retirer;
    {
        Ref<Resource> ref = make_ref<Resource>();
        ref->set_retirer(&retirer);
        Ref<Resource> copy = ref;
        ref.reset();
        CHECK(retirer.retired == 0);
    }
    CHECK(retirer.retired == 1);
    CHECK(g_destroyed == 0);

    retirer.destroy_pending();
    CHECK(g_destroyed == 1);
}

TEST_CASE("allocate_ref frees the block when the constructor throws")
{
    MemoryStats before = test::all_allocations();
    CHECK_THROWS_AS(make_ref<ThrowingResource>(), std::runtime_error);
    MemoryStats after = test::all_allocations();
    CHECK(after.live_bytes == before.live_bytes);
}

TEST_CASE("Concurrent copies and releases leave one reference and destroy once")
{
    g_destroyed = 0;
    constexpr int32_t thread_count = 8;
    constexpr int32_t iterations = 100000;

    Ref<Resource> shared = make_ref<Resource>();
    std::vector<std::thread> threads;
    for (int32_t i = 0; i < thread_count; ++i)
    {
        threads.emplace_back([&shared]
        {
            for (int32_t n = 0; n < iterations; ++n)
            {
                Ref<Resource> copy = shared;
            }
        });
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }

    CHECK(shared->ref_count() == 1);
    CHECK(g_destroyed == 0);
    shared.reset();
    CHECK(g_destroyed == 1);
}

}
