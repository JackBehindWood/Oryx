#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Renderer/RendererContext.h"
#include "unit/MemoryTestSupport.h"
#include "unit/Renderer/FakeFontSource.h"

using namespace oryx;

namespace
{

const Colour RED = { 1.0f, 0.0f, 0.0f, 1.0f };
const Colour BLUE = { 0.0f, 0.0f, 1.0f, 1.0f };

struct ReplayFixture
{
    UniquePtr<RendererContext> context = create_renderer_context({ RHIBackend::Null });
    std::vector<DrawItem> sink;
    test::FakeFontSource* source = nullptr;
    Font font = test::make_fake_font(source);
    Camera2D camera = Camera2D::screen_space(100.0f, 50.0f);
    BatchRenderer2D batcher{ batch_renderer_desc(*context, sink) };
    ReplayTarget target{ { 0.0f, 0.0f }, { 100.0f, 50.0f } };

    void run(const DrawList& list)
    {
        batcher.begin(camera, { sink, { RHIFormat::BGRA8Unorm, RHIFormat::Undefined }, { 100.0f, 50.0f } });
        replay(list, batcher, font, target);
        batcher.end();
    }

    std::vector<Vertex2DUi> quads(const DrawItem& item)
    {
        std::vector<Vertex2DUi> result(item.vertex_count);
        std::memcpy(result.data(), item.vertex_buffers[0]->map() + item.vertex_offsets[0], item.vertex_count * sizeof(Vertex2DUi));
        return result;
    }

    ~ReplayFixture() { context->rhi->wait_idle(); }
};

} // namespace

TEST_CASE("replay: a rect lands flipped into the bottom-left screen space")
{
    ReplayFixture f;
    DrawList list;
    list.add_rect({ { 10.0f, 5.0f }, { 20.0f, 10.0f } }, RED);
    f.run(list);
    REQUIRE(f.sink.size() == 1);
    const std::vector<Vertex2DUi> vertices = f.quads(f.sink[0]);
    REQUIRE(vertices.size() == 4);
    float min_x = 1000.0f, max_x = -1000.0f, min_y = 1000.0f, max_y = -1000.0f;
    for (const Vertex2DUi& vertex : vertices)
    {
        min_x = std::min(min_x, vertex.position[0]);
        max_x = std::max(max_x, vertex.position[0]);
        min_y = std::min(min_y, vertex.position[1]);
        max_y = std::max(max_y, vertex.position[1]);
    }
    CHECK(min_x == doctest::Approx(10.0f));
    CHECK(max_x == doctest::Approx(30.0f));
    CHECK(min_y == doctest::Approx(35.0f));
    CHECK(max_y == doctest::Approx(45.0f));
}

TEST_CASE("replay: the target origin offsets the whole list")
{
    ReplayFixture f;
    f.target.origin = { 40.0f, 10.0f };
    DrawList list;
    list.add_rect({ { 0.0f, 0.0f }, { 10.0f, 10.0f } }, RED);
    f.run(list);
    const std::vector<Vertex2DUi> vertices = f.quads(f.sink[0]);
    float min_x = 1000.0f, max_y = -1000.0f;
    for (const Vertex2DUi& vertex : vertices)
    {
        min_x = std::min(min_x, vertex.position[0]);
        max_y = std::max(max_y, vertex.position[1]);
    }
    CHECK(min_x == doctest::Approx(40.0f));
    CHECK(max_y == doctest::Approx(40.0f));
}

TEST_CASE("replay: a clip becomes one scissor and returns to none")
{
    ReplayFixture f;
    DrawList list;
    list.add_rect({ { 0.0f, 0.0f }, { 4.0f, 4.0f } }, RED);
    list.push_clip({ { 0.0f, 0.0f }, { 10.0f, 10.0f } });
    list.add_rect({ { 2.0f, 2.0f }, { 4.0f, 4.0f } }, RED);
    list.add_rect({ { 4.0f, 4.0f }, { 4.0f, 4.0f } }, RED);
    list.pop_clip();
    list.add_rect({ { 8.0f, 8.0f }, { 4.0f, 4.0f } }, RED);
    f.run(list);
    REQUIRE(f.sink.size() == 3);
    CHECK_FALSE(f.sink[0].has_scissor);
    REQUIRE(f.sink[1].has_scissor);
    CHECK(f.sink[1].scissor.x == 0);
    CHECK(f.sink[1].scissor.y == 0);
    CHECK(f.sink[1].scissor.width == 10);
    CHECK(f.sink[1].scissor.height == 10);
    CHECK_FALSE(f.sink[2].has_scissor);
    CHECK(f.batcher.clip_depth() == 0);
}

TEST_CASE("replay: nested clips intersect")
{
    ReplayFixture f;
    DrawList list;
    list.push_clip({ { 0.0f, 0.0f }, { 20.0f, 20.0f } });
    list.push_clip({ { 10.0f, 10.0f }, { 20.0f, 20.0f } });
    list.add_rect({ { 12.0f, 12.0f }, { 2.0f, 2.0f } }, RED);
    list.pop_clip();
    list.pop_clip();
    f.run(list);
    REQUIRE(f.sink.size() == 1);
    REQUIRE(f.sink[0].has_scissor);
    CHECK(f.sink[0].scissor.x == 10);
    CHECK(f.sink[0].scissor.y == 10);
    CHECK(f.sink[0].scissor.width == 10);
    CHECK(f.sink[0].scissor.height == 10);
}

TEST_CASE("replay: merged channels draw later channels last")
{
    ReplayFixture f;
    DrawList list;
    list.split_channels(2);
    list.set_channel(1);
    list.add_rect({ { 0.0f, 0.0f }, { 5.0f, 5.0f } }, BLUE);
    list.set_channel(0);
    list.add_rect({ { 0.0f, 0.0f }, { 5.0f, 5.0f } }, RED);
    list.merge();
    f.run(list);
    REQUIRE(f.sink.size() == 1);
    const std::vector<Vertex2DUi> vertices = f.quads(f.sink[0]);
    REQUIRE(vertices.size() == 8);
    CHECK(vertices[0].colour[0] == 255);
    CHECK(vertices[4].colour[2] == 255);
}

TEST_CASE("replay: unmerged channels replay in channel order too")
{
    ReplayFixture f;
    DrawList list;
    list.split_channels(2);
    list.set_channel(1);
    list.add_rect({ { 0.0f, 0.0f }, { 5.0f, 5.0f } }, BLUE);
    list.set_channel(0);
    list.add_rect({ { 0.0f, 0.0f }, { 5.0f, 5.0f } }, RED);
    f.run(list);
    const std::vector<Vertex2DUi> vertices = f.quads(f.sink[0]);
    CHECK(vertices[0].colour[0] == 255);
    CHECK(vertices[4].colour[2] == 255);
}

TEST_CASE("replay: a rounded rect is one shape quad and a square radius is a plain quad")
{
    ReplayFixture f;
    DrawList list;
    list.add_rounded_rect({ { 0.0f, 0.0f }, { 20.0f, 20.0f } }, uniform_radius(6.0f), RED);
    f.run(list);
    CHECK(f.batcher.stats().primitives == 1);
    CHECK(f.sink.size() == 1);

    ReplayFixture square;
    DrawList flat;
    flat.add_rounded_rect({ { 0.0f, 0.0f }, { 20.0f, 20.0f } }, {}, RED);
    square.run(flat);
    CHECK(square.batcher.stats().triangles == 2);
}

TEST_CASE("replay: borders and lines share one batch with the shapes around them")
{
    ReplayFixture f;
    DrawList list;
    list.add_border({ { 0.0f, 0.0f }, { 20.0f, 20.0f } }, {}, 2.0f, RED);
    list.add_line({ 0.0f, 0.0f }, { 10.0f, 0.0f }, 2.0f, BLUE);
    f.run(list);
    CHECK(f.batcher.stats().primitives == 2);
    CHECK(f.sink.size() == 1);
}

TEST_CASE("replay: text draws glyph quads and is skipped while the font is not ready")
{
    ReplayFixture f;
    DrawList list;
    list.add_text({ 5.0f, 20.0f }, "AB", 16.0f, TextAlign::Left, RED);
    f.run(list);
    CHECK(f.batcher.stats().primitives == 2);

    ReplayFixture idle;
    idle.source->is_ready = false;
    idle.run(list);
    CHECK(idle.sink.empty());
}

TEST_CASE("replay: a warm replay allocates nothing")
{
    ReplayFixture f;
    DrawList list;
    list.push_clip({ { 0.0f, 0.0f }, { 50.0f, 50.0f } });
    list.add_rect({ { 0.0f, 0.0f }, { 4.0f, 4.0f } }, RED);
    list.add_text({ 5.0f, 20.0f }, "AB", 16.0f, TextAlign::Left, RED);
    list.pop_clip();
    f.run(list);
    f.sink.clear();
    f.run(list);
    f.sink.clear();
    MemoryStats before = test::all_allocations();
    f.run(list);
    CHECK(memory_delta(before, test::all_allocations()).allocation_count == 0);
}

TEST_CASE("replay: an image draws its table entry, a missing one nothing")
{
    ReplayFixture f;
    const Texture2D picture = Texture2D::create(*f.context->rhi, { .width = 1, .height = 1 });
    DrawList list;
    list.add_image({ { 10.0f, 5.0f }, { 20.0f, 10.0f } }, ImageHandle{ 0 }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, {}, RED);
    list.add_image({ { 10.0f, 5.0f }, { 20.0f, 10.0f } }, ImageHandle{ 7 }, { 0.0f, 0.0f }, { 1.0f, 1.0f }, {}, RED);
    const Texture2D* const images[1] = { &picture };
    f.target.images = images;
    f.target.image_count = 1;
    f.run(list);
    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_count == 4);
}

TEST_CASE("replay: the single-texture overload serves handle zero")
{
    ReplayFixture f;
    const Texture2D picture = Texture2D::create(*f.context->rhi, { .width = 1, .height = 1 });
    DrawList list;
    list.add_image({ { 0.0f, 0.0f }, { 10.0f, 10.0f } }, k_single_image, { 0.0f, 0.0f }, { 1.0f, 1.0f }, {}, RED);
    f.batcher.begin(f.camera, { f.sink, { RHIFormat::BGRA8Unorm, RHIFormat::Undefined }, { 100.0f, 50.0f } });
    replay(list, f.batcher, f.font, f.target, picture);
    f.batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_count == 4);
}

TEST_CASE("replay: a rounded image is a fan of textured quads whose uvs follow the picture")
{
    ReplayFixture f;
    const Texture2D picture = Texture2D::create(*f.context->rhi, { .width = 1, .height = 1 });
    DrawList list;
    list.add_image({ { 0.0f, 0.0f }, { 20.0f, 20.0f } }, k_single_image, { 0.0f, 0.0f }, { 1.0f, 1.0f }, uniform_radius(10.0f), RED);
    f.target.images = nullptr;
    f.batcher.begin(f.camera, { f.sink, { RHIFormat::BGRA8Unorm, RHIFormat::Undefined }, { 100.0f, 50.0f } });
    replay(list, f.batcher, f.font, f.target, picture);
    f.batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_count > 4);
    std::vector<Vertex2DQuad> vertices(f.sink[0].vertex_count);
    std::memcpy(vertices.data(), f.sink[0].vertex_buffers[0]->map() + f.sink[0].vertex_offsets[0], vertices.size() * sizeof(Vertex2DQuad));
    for (const Vertex2DQuad& vertex : vertices)
    {
        CHECK(vertex.uv[0] >= -0.001f);
        CHECK(vertex.uv[0] <= 1.001f);
        CHECK(vertex.uv[1] >= -0.001f);
        CHECK(vertex.uv[1] <= 1.001f);
    }
}

TEST_CASE("replay: rects, rounded shapes, lines and text draw in one batch in command order")
{
    ReplayFixture f;
    DrawList list;
    list.add_rect({ { 0.0f, 0.0f }, { 4.0f, 4.0f } }, RED);
    list.add_rounded_rect({ { 10.0f, 0.0f }, { 20.0f, 20.0f } }, uniform_radius(5.0f), BLUE);
    list.add_text({ 5.0f, 20.0f }, "A", 16.0f, TextAlign::Left, RED);
    list.add_line({ 0.0f, 0.0f }, { 10.0f, 0.0f }, 2.0f, BLUE);
    list.add_border({ { 0.0f, 0.0f }, { 20.0f, 20.0f } }, uniform_radius(5.0f), 1.0f, RED);
    f.run(list);
    REQUIRE(f.sink.size() == 1);
    const std::vector<Vertex2DUi> vertices = f.quads(f.sink[0]);
    REQUIRE(vertices.size() == 20);
    CHECK(vertices[0].control[1] == static_cast<uint8_t>(UiMode::Texture));
    CHECK(vertices[4].control[1] == static_cast<uint8_t>(UiMode::RoundedFill));
    CHECK(vertices[8].control[1] == static_cast<uint8_t>(UiMode::Coverage));
    CHECK(vertices[12].control[1] == static_cast<uint8_t>(UiMode::Texture));
    CHECK(vertices[16].control[1] == static_cast<uint8_t>(UiMode::RoundedBorder));
    CHECK(f.batcher.stats().flushes[static_cast<uint32_t>(FlushReason::StreamChange)] == 0);
}

TEST_CASE("replay: a rounded shape carries its radii and thickness as fractions of the shorter half-extent")
{
    ReplayFixture f;
    DrawList list;
    list.add_border({ { 0.0f, 0.0f }, { 40.0f, 20.0f } }, uniform_radius(5.0f), 2.0f, RED);
    f.run(list);
    const std::vector<Vertex2DUi> vertices = f.quads(f.sink[0]);
    REQUIRE(vertices.size() == 4);
    CHECK(vertices[0].radii[0] == 128);
    CHECK(vertices[0].control[2] == 51);
    CHECK(std::abs(vertices[0].uv[0]) == doctest::Approx(20.0f + UI_SHAPE_MARGIN));
    CHECK(std::abs(vertices[0].uv[1]) == doctest::Approx(10.0f + UI_SHAPE_MARGIN));
}
