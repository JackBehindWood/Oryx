#include "doctest.h"

#include "Oryx.h"

#ifdef OX_ENABLE_GRAPHICS

#include "Oasis/Core/GuiShowcase.h"
#include "unit/MemoryTestSupport.h"
#include "NullRHI.h"
#include "Oryx/Renderer/RendererContext.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;

namespace
{

struct ShowcaseFixture
{
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    oasis::GuiShowcase showcase;
    oasis::GuiShowcase::Numbers numbers;
    ImInput input;

    ShowcaseFixture()
    {
        showcase.set_font(&font);
        input.surface_size = { 800.0f, 600.0f };
        input.delta_time = 1.0f / 60.0f;
        numbers.frame.batch.draws = 7;
        numbers.pipelines.entries = 3;
    }

    void frames(uint32_t count)
    {
        for (uint32_t index = 0; index < count; ++index)
        {
            showcase.run(input, numbers);
            input.pointer.buttons[0].pressed = false;
            input.pointer.buttons[0].released = false;
        }
    }

    const LayoutNode* find(std::string_view text) const
    {
        const LayoutTree& layout = showcase.context().layout();
        for (uint32_t index = 0; index < layout.node_count(); ++index)
        {
            if (layout.node(index).paint.text == text)
            {
                return &layout.node(index);
            }
        }
        return nullptr;
    }

    void click(std::string_view text)
    {
        const LayoutNode* node = find(text);
        REQUIRE(node != nullptr);
        input.pointer.valid = true;
        input.pointer.position = rect_centre(node->rect);
        frames(1);
        input.pointer.buttons[0] = { true, true, false };
        frames(1);
        input.pointer.buttons[0] = { false, false, true };
        frames(1);
        input.pointer.valid = false;
        frames(3);
    }
};

struct ReplayRig
{
    UniquePtr<RendererContext> context = create_renderer_context({ RHIBackend::Null });
    std::vector<DrawItem> sink;
    Camera2D camera = Camera2D::screen_space(800.0f, 600.0f);
    BatchRenderer2D batcher{ batch_renderer_desc(*context, sink) };

    BatchStats replay_once(ShowcaseFixture& f)
    {
        sink.clear();
        batcher.recycle(0);
        batcher.begin(camera, { sink, { RHIFormat::BGRA8Unorm, RHIFormat::Undefined }, { 800.0f, 600.0f } });
        const BatchStats before = batcher.stats();
        replay(f.showcase.context().draw_list(), batcher, f.font, { { 0.0f, 0.0f }, { 800.0f, 600.0f } });
        const BatchStats delta = batch_stats_delta(batcher.stats(), before);
        batcher.end();
        return delta;
    }

    ~ReplayRig() { context->rhi->wait_idle(); }
};

} // namespace

TEST_CASE("GUI showcase: the perf tab's replay is attributed per stream")
{
    ReplayRig rig;
    ShowcaseFixture f;
    f.frames(4);
    const BatchStats stats = rig.replay_once(f);
    const ImStats& gui = f.showcase.context().stats();
    MESSAGE("shapes: " << gui.rects << " rect, " << gui.rounded_rects << " rounded, " << gui.borders << " border, " << gui.lines << " line, " << gui.texts << " text");
    MESSAGE("total: " << stats.primitives << " prims, " << stats.vertices << " verts, " << stats.bytes << " B, " << stats.draws << " draws, " << stats.triangles << " tris");
    uint32_t primitives = 0;
    uint32_t vertices = 0;
    uint32_t bytes = 0;
    uint32_t draws = 0;
    for (uint32_t stream = 0; stream < PRIMITIVE_2D_COUNT; ++stream)
    {
        const BatchStreamStats& s = stats.streams[stream];
        MESSAGE(std::string(primitive_name(static_cast<Primitive2D>(stream))) << ": " << s.primitives << " prims, " << s.vertices << " verts, " << s.bytes << " B, " << s.draws << " draws");
        primitives += s.primitives;
        vertices += s.vertices;
        bytes += s.bytes;
        draws += s.draws;
    }
    for (uint32_t reason = 0; reason < FLUSH_REASON_COUNT; ++reason)
    {
        MESSAGE("flush reason " << reason << ": " << stats.flushes[reason]);
    }
    uint32_t switches = 0;
    for (uint32_t from = 0; from < PRIMITIVE_2D_COUNT; ++from)
    {
        for (uint32_t to = 0; to < PRIMITIVE_2D_COUNT; ++to)
        {
            if (stats.stream_switches[from][to] != 0)
            {
                const std::string line = std::string(primitive_name(static_cast<Primitive2D>(from))) + " > " + primitive_name(static_cast<Primitive2D>(to)) + ": " + std::to_string(stats.stream_switches[from][to]);
                MESSAGE(line);
            }
            switches += stats.stream_switches[from][to];
        }
    }
    CHECK(primitives == stats.primitives);
    CHECK(vertices == stats.vertices);
    CHECK(bytes == stats.bytes);
    CHECK(draws == stats.draws);
    CHECK(switches == stats.flushes[static_cast<uint32_t>(FlushReason::StreamChange)]);
    CHECK(stats.draws == rig.sink.size());
    CHECK(stats.primitives > 0);
    CHECK(stats.streams[static_cast<uint32_t>(Primitive2D::Ui)].primitives == stats.primitives);
    CHECK(switches == 0);
    CHECK(stats.draws <= stats.flushes[static_cast<uint32_t>(FlushReason::ScissorChange)] + 1);
}

TEST_CASE("GUI showcase: replaying the same frame twice gives identical stats")
{
    ReplayRig rig;
    ShowcaseFixture f;
    f.frames(4);
    const BatchStats first = rig.replay_once(f);
    const BatchStats second = rig.replay_once(f);
    CHECK(std::memcmp(&first, &second, sizeof(BatchStats)) == 0);
}

TEST_CASE("GUI showcase: every tab builds and the perf tab shows the renderer numbers")
{
    ShowcaseFixture f;
    f.frames(4);
    REQUIRE(f.find("draws") != nullptr);
    REQUIRE(f.find("pipelines") != nullptr);
    CHECK(f.find("7") != nullptr);
    f.click("Widgets");
    CHECK(f.find("checkbox") != nullptr);
    f.click("Data");
    CHECK(f.find("policy series") != nullptr);
    CHECK(f.find("Visits") != nullptr);
    f.click("Visits");
    CHECK(f.find("Visits ^") != nullptr);
    f.click("Perf");
    CHECK(f.find("draws") != nullptr);
}

TEST_CASE("GUI showcase: warm frames allocate nothing on any tab")
{
    ShowcaseFixture f;
    f.frames(3);
    for (const char* tab : { "Perf", "Widgets", "Data" })
    {
        f.click(tab);
        f.frames(6);
        const MemoryStats before = test::all_allocations();
        f.frames(3);
        CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
    }
}

#endif
