#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

class Probe : public RHIResource
{
public:
    explicit Probe(std::atomic<int32_t>& destroyed, Ref<Probe> child = {})
        : m_destroyed(destroyed)
        , m_child(std::move(child))
    {
    }

    ~Probe() override { ++m_destroyed; }

private:
    std::atomic<int32_t>& m_destroyed;
    Ref<Probe> m_child;
};

class OrderedProbe : public RHIResource
{
public:
    OrderedProbe(std::vector<int32_t>& log, int32_t id)
        : m_log(log)
        , m_id(id)
    {
    }

    ~OrderedProbe() override { m_log.push_back(m_id); }

private:
    std::vector<int32_t>& m_log;
    int32_t m_id;
};

} // namespace

TEST_CASE("Retired resources are destroyed immediately when no device exists")
{
    std::atomic<int32_t> destroyed{ 0 };
    Ref<Probe> probe = make_ref<Probe>(destroyed);
    probe.reset();
    CHECK(destroyed == 1);
    CHECK(RHIResource::live_count() == 0);
}

TEST_CASE("A released resource waits until its device completes the frame")
{
    RHIDeviceLease lease;
    std::atomic<int32_t> destroyed{ 0 };
    Ref<Probe> probe = make_ref<Probe>(destroyed);
    CHECK(RHIResource::live_count() == 1);

    probe.reset();
    CHECK(destroyed == 0);
    CHECK(RHIResource::retired_pending() == 1);

    const uint64_t ended = lease.end_frame();
    lease.complete(ended);
    CHECK(destroyed == 1);
    CHECK(RHIResource::retired_pending() == 0);
    CHECK(RHIResource::live_count() == 0);
}

TEST_CASE("Collection frees only frames up to the completed one")
{
    RHIDeviceLease lease;
    std::atomic<int32_t> destroyed{ 0 };

    make_ref<Probe>(destroyed);
    const uint64_t first = lease.end_frame();
    make_ref<Probe>(destroyed);
    const uint64_t second = lease.end_frame();
    make_ref<Probe>(destroyed);
    CHECK(RHIResource::retired_pending() == 3);

    lease.complete(first);
    CHECK(destroyed == 1);
    lease.complete(second);
    CHECK(destroyed == 2);
    lease.complete(second);
    CHECK(destroyed == 2);
    lease.set_idle();
    CHECK(destroyed == 3);
}

TEST_CASE("Collection destroys children released by a retired parent")
{
    RHIDeviceLease lease;
    std::atomic<int32_t> destroyed{ 0 };
    Ref<Probe> child = make_ref<Probe>(destroyed);
    Ref<Probe> parent = make_ref<Probe>(destroyed, std::move(child));

    parent.reset();
    lease.set_idle();
    CHECK(destroyed == 2);
    CHECK(RHIResource::retired_pending() == 0);
}

TEST_CASE("Collection pops prefixes in order across many frames")
{
    RHIDeviceLease lease;
    std::vector<int32_t> order;
    std::vector<uint64_t> frames;
    for (int32_t frame = 0; frame < 50; ++frame)
    {
        for (int32_t i = 0; i < 3; ++i)
        {
            make_ref<OrderedProbe>(order, frame * 3 + i);
        }
        frames.push_back(lease.end_frame());
    }
    CHECK(RHIResource::retired_pending() == 150);
    for (size_t frame = 0; frame < frames.size(); ++frame)
    {
        lease.complete(frames[frame]);
        CHECK(RHIResource::retired_pending() == (49 - frame) * 3);
    }
    REQUIRE(order.size() == 150);
    for (int32_t i = 0; i < 150; ++i)
    {
        CHECK(order[static_cast<size_t>(i)] == i);
    }
}

TEST_CASE("The last lease drains everything still retired")
{
    std::atomic<int32_t> destroyed{ 0 };
    {
        RHIDeviceLease lease;
        for (int32_t i = 0; i < 3; ++i)
        {
            make_ref<Probe>(destroyed);
            lease.end_frame();
        }
        CHECK(destroyed == 0);
    }
    CHECK(destroyed == 3);
    CHECK(RHIResource::retired_pending() == 0);
}

TEST_CASE("A busy device holds back resources of another until it completes")
{
    RHIDeviceLease a;
    RHIDeviceLease b;
    std::atomic<int32_t> destroyed{ 0 };

    b.complete(b.end_frame());
    make_ref<Probe>(destroyed);
    const uint64_t frame = a.end_frame();

    a.complete(frame);
    CHECK(destroyed == 0);
    b.complete(frame);
    CHECK(destroyed == 1);
}

TEST_CASE("A device that has begun work and completed nothing holds back the current frame")
{
    RHIDeviceLease busy;
    RHIDeviceLease other;
    std::atomic<int32_t> destroyed{ 0 };

    busy.begin_work();
    make_ref<Probe>(destroyed);
    other.set_idle();
    CHECK(destroyed == 0);

    busy.complete(busy.end_frame());
    CHECK(destroyed == 1);
}

TEST_CASE("An idle device does not block collection")
{
    RHIDeviceLease busy;
    RHIDeviceLease other;
    std::atomic<int32_t> destroyed{ 0 };

    other.complete(other.end_frame());
    make_ref<Probe>(destroyed);
    const uint64_t frame = busy.end_frame();
    busy.complete(frame);
    CHECK(destroyed == 0);
    other.set_idle();
    CHECK(destroyed == 1);
}

TEST_CASE("Concurrent release while collecting destroys every resource once")
{
    RHIDeviceLease lease;
    std::atomic<int32_t> destroyed{ 0 };

    constexpr int32_t THREADS = 4;
    constexpr int32_t PER_THREAD = 500;
    std::vector<std::vector<Ref<Probe>>> owned(THREADS);
    for (std::vector<Ref<Probe>>& objects : owned)
    {
        for (int32_t i = 0; i < PER_THREAD; ++i)
        {
            objects.push_back(make_ref<Probe>(destroyed));
        }
    }

    std::atomic<bool> go{ false };
    std::vector<std::thread> threads;
    for (std::vector<Ref<Probe>>& objects : owned)
    {
        threads.emplace_back([&objects, &go] {
            while (!go.load())
            {
            }
            objects.clear();
        });
    }
    go = true;
    for (int32_t i = 0; i < 200; ++i)
    {
        lease.complete(lease.end_frame());
    }
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    lease.set_idle();
    CHECK(destroyed.load() == THREADS * PER_THREAD);
    CHECK(RHIResource::retired_pending() == 0);
}
