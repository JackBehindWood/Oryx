#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Graphics/RHI/Detail/RHICommandStream.h"
#include "Oryx/Graphics/RHI/Detail/RHIResourceRetainer.h"

using namespace oryx;

namespace
{

uint32_t draw_counts(const RHICommandStream& stream, std::vector<uint32_t>& out)
{
    for (const RHICommand& command : stream)
    {
        if (const RHIDrawCommand* draw = command_cast<RHIDrawCommand>(command))
        {
            out.push_back(draw->vertex_count());
        }
    }
    return static_cast<uint32_t>(out.size());
}

} // namespace

TEST_CASE("RHICommandStream iterates commands in recording order with their types")
{
    RHICommandStream stream;
    CHECK(stream.size() == 0);
    CHECK(stream.begin() == stream.end());

    stream.emplace<RHIDrawCommand>(1u, 1u, 0u, 0u);
    stream.emplace<RHIPopDebugGroupCommand>();
    stream.emplace<RHIDrawCommand>(2u, 1u, 0u, 0u);
    CHECK(stream.size() == 3);

    std::vector<std::string> names;
    for (const RHICommand& command : stream)
    {
        names.push_back(command.command_name());
    }
    CHECK(names == std::vector<std::string>{ "Draw", "PopDebugGroup", "Draw" });

    std::vector<uint32_t> counts;
    draw_counts(stream, counts);
    CHECK(counts == std::vector<uint32_t>{ 1, 2 });
}

TEST_CASE("RHICommandStream gives each command type its own id")
{
    RHICommandStream stream;
    RHICommand& draw_a = stream.emplace<RHIDrawCommand>(1u, 1u, 0u, 0u);
    RHICommand& pop = stream.emplace<RHIPopDebugGroupCommand>();
    RHICommand& draw_b = stream.emplace<RHIDrawCommand>(1u, 1u, 0u, 0u);
    CHECK(draw_a.type_id() == draw_b.type_id());
    CHECK(draw_a.type_id() != pop.type_id());
    CHECK(command_cast<RHIDrawCommand>(pop) == nullptr);
    CHECK(command_cast<RHIPopDebugGroupCommand>(pop) != nullptr);
}

TEST_CASE("RHICommandStream aligns commands and payloads")
{
    RHICommandStream stream;
    for (uint32_t i = 0; i < 50; ++i)
    {
        stream.emplace<RHIPopDebugGroupCommand>();
        void* payload = stream.allocate(1 + i % 7, 16);
        CHECK(reinterpret_cast<uintptr_t>(payload) % 16 == 0);
        stream.emplace<RHIDrawCommand>(i, 1u, 0u, 0u);
    }
    for (const RHICommand& command : stream)
    {
        CHECK(reinterpret_cast<uintptr_t>(&command) % alignof(RHICommand) == 0);
    }
}

TEST_CASE("RHICommandStream grows past one block and is reusable after clear")
{
    RHICommandStream stream(256);
    for (int32_t round = 0; round < 2; ++round)
    {
        for (uint32_t i = 0; i < 2000; ++i)
        {
            stream.emplace<RHIDrawCommand>(i, 1u, 0u, 0u);
        }
        CHECK(stream.size() == 2000);
        std::vector<uint32_t> counts;
        draw_counts(stream, counts);
        REQUIRE(counts.size() == 2000);
        CHECK(counts.front() == 0);
        CHECK(counts.back() == 1999);
        stream.clear();
        CHECK(stream.size() == 0);
        CHECK(stream.begin() == stream.end());
    }
}

TEST_CASE("RHICommandStream moves its commands and leaves the source empty")
{
    RHICommandStream source;
    source.emplace<RHIDrawCommand>(7u, 1u, 0u, 0u);
    source.emplace<RHIDrawCommand>(8u, 1u, 0u, 0u);

    RHICommandStream moved = std::move(source);
    CHECK(moved.size() == 2);
    CHECK(source.size() == 0);
    CHECK(source.begin() == source.end());

    std::vector<uint32_t> counts;
    draw_counts(moved, counts);
    CHECK(counts == std::vector<uint32_t>{ 7, 8 });

    RHICommandStream assigned;
    assigned.emplace<RHIPopDebugGroupCommand>();
    assigned = std::move(moved);
    CHECK(assigned.size() == 2);
    CHECK(moved.size() == 0);

    source.emplace<RHIDrawCommand>(9u, 1u, 0u, 0u);
    CHECK(source.size() == 1);
}

TEST_CASE("RHIResourceRetainer keeps each resource once inside its scan window")
{
    NullRHI rhi;
    RHIBufferPtr a = rhi.create_buffer({ .size = 16 });
    RHIResourceRetainer retainer;
    retainer.retain(*a);
    retainer.retain(*a);
    CHECK(retainer.size() == 1);

    std::vector<RHIBufferPtr> filler;
    for (size_t i = 0; i < RHIResourceRetainer::SCAN_WINDOW; ++i)
    {
        filler.push_back(rhi.create_buffer({ .size = 16 }));
        retainer.retain(*filler.back());
    }
    CHECK(retainer.size() == 1 + RHIResourceRetainer::SCAN_WINDOW);
    retainer.retain(*a);
    CHECK(retainer.size() == 2 + RHIResourceRetainer::SCAN_WINDOW);
    retainer.retain(*filler.back());
    CHECK(retainer.size() == 2 + RHIResourceRetainer::SCAN_WINDOW);
}

TEST_CASE("RHIResourceRetainer slot and binding caches skip repeats")
{
    NullRHI rhi;
    RHIBufferPtr a = rhi.create_buffer({ .size = 16 });
    RHIBufferPtr b = rhi.create_buffer({ .size = 16 });
    RHIResourceRetainer retainer;
    retainer.retain_in_slot(RHIResourceRetainer::SLOT_VERTEX, *a);
    retainer.retain_in_slot(RHIResourceRetainer::SLOT_VERTEX, *a);
    CHECK(retainer.size() == 1);
    retainer.retain_in_slot(RHIResourceRetainer::SLOT_VERTEX, *b);
    CHECK(retainer.size() == 2);
    retainer.retain_for_binding(3, *a);
    retainer.retain_for_binding(3, *a);
    CHECK(retainer.size() == 2);
}

TEST_CASE("RHIResourceRetainer keeps resources alive until drained")
{
    NullRHI rhi;
    const size_t baseline = RHIResource::live_count();
    RHIResourceRetainer retainer;
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 16 });
        retainer.retain(*buffer);
    }
    CHECK(RHIResource::live_count() == baseline + 1);

    std::vector<Ref<RHIResource>> sink;
    retainer.drain_into(sink);
    CHECK(sink.size() == 1);
    CHECK(retainer.size() == 0);
    sink.clear();
    rhi.wait_idle();
    rhi.end_frame();
    rhi.end_frame();
    rhi.end_frame();
    CHECK(RHIResource::live_count() == baseline);
}

TEST_CASE("RHIResourceRetainer moves with its references")
{
    NullRHI rhi;
    RHIBufferPtr a = rhi.create_buffer({ .size = 16 });
    RHIResourceRetainer source;
    source.retain_in_slot(RHIResourceRetainer::SLOT_INDEX, *a);

    RHIResourceRetainer moved = std::move(source);
    CHECK(moved.size() == 1);
    CHECK(source.size() == 0);
    moved.retain_in_slot(RHIResourceRetainer::SLOT_INDEX, *a);
    CHECK(moved.size() == 1);

    RHIResourceRetainer assigned;
    assigned = std::move(moved);
    CHECK(assigned.size() == 1);
    CHECK(moved.size() == 0);
}
