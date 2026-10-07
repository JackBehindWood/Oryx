#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Renderer/RendererContext.h"

using namespace oryx;

namespace
{

const Colour WHITE = { 1.0f, 1.0f, 1.0f, 1.0f };
const Colour RED = { 1.0f, 0.0f, 0.0f, 1.0f };

struct BatchFixture
{
    UniquePtr<RendererContext> context = create_renderer_context({ RHIBackend::Null });
    std::vector<DrawItem> sink;
    Camera2D camera{ 2.0f, 2.0f };

    BatchRendererDesc desc() { return batch_renderer_desc(*context, sink); }

    Texture2D texture() { return Texture2D::create(*context->rhi, { .width = 1, .height = 1 }); }

    ~BatchFixture() { context->rhi->wait_idle(); }
};

class ExtraStreamBatcher : public BatchRenderer
{
public:
    explicit ExtraStreamBatcher(const BatchRendererDesc& desc)
        : BatchRenderer(desc)
        , m_stream(register_stream({ sizeof(Vertex2DLine), 2, 0, false, &pipeline_def(Primitive2D::Line) }))
    {
    }

    void line()
    {
        select(m_stream, {});
        const Vertex2DLine vertices[2] = {};
        std::memcpy(append(), vertices, sizeof(vertices));
    }

private:
    BatchStreamId m_stream;
};

} // namespace

TEST_CASE("BatchRenderer2D: quads share one draw with indices from the shared buffer")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    batcher.begin(f.camera);
    for (uint32_t i = 0; i < 10; ++i)
    {
        batcher.draw_rect({ static_cast<float>(i), 0.0f }, { 1.0f, 1.0f }, RED);
    }
    CHECK(f.sink.empty());
    batcher.end();

    REQUIRE(f.sink.size() == 1);
    const DrawItem& item = f.sink[0];
    CHECK(item.index_buffer == f.context->defaults.quad_indices);
    CHECK(item.index_count == 60);
    CHECK(item.vertex_count == 40);
    CHECK(item.base_vertex == 0);
    CHECK(item.vertex_buffers[0].get() != nullptr);
    CHECK(item.texture_count == 1);
    CHECK(item.textures[0] == f.context->defaults.white_texture);
    CHECK(batcher.stats().draws == 1);
    CHECK(batcher.stats().primitives == 10);
    CHECK(batcher.stats().flushes[static_cast<uint32_t>(FlushReason::End)] == 1);
}

TEST_CASE("BatchRenderer2D: an empty scene records nothing and flush needs a scene")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    CHECK_THROWS_AS(batcher.flush(), Error);
    CHECK_THROWS_AS(batcher.end(), Error);
    batcher.begin(f.camera);
    batcher.flush();
    batcher.end();
    CHECK(f.sink.empty());
    CHECK(batcher.stats().draws == 0);
}

TEST_CASE("BatchRenderer2D: scene errors")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    CHECK_THROWS_AS(batcher.draw_rect({}, { 1.0f, 1.0f }, WHITE), Error);
    CHECK_THROWS_AS(batcher.draw_line({}, { 1.0f, 1.0f }, WHITE), Error);
    batcher.begin(f.camera);
    CHECK(batcher.open());
    CHECK_THROWS_AS(batcher.begin(f.camera), Error);
    batcher.end();
    CHECK_FALSE(batcher.open());
}

TEST_CASE("BatchRenderer2D: a stream or sampler change flushes in submission order")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Texture2D checker = f.texture();
    batcher.begin(f.camera);
    batcher.draw_sprite({ 0.0f, 0.0f }, { 1.0f, 1.0f }, checker);
    batcher.draw_sprite({ 1.0f, 0.0f }, { 1.0f, 1.0f }, checker);
    batcher.draw_circle({ 0.0f, 0.0f }, 1.0f, RED);
    batcher.draw_sprite({ 2.0f, 0.0f }, { 1.0f, 1.0f }, checker);
    batcher.draw_rect({ 3.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    batcher.draw_line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    batcher.draw_triangle({ 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f }, RED);
    batcher.end();

    REQUIRE(f.sink.size() == 6);
    const GraphicsPipelineHandle quad = f.context->pipeline_memo.get(*f.context->rhi, f.context->pipelines, f.context->shaders, f.context->back_buffer_format, pipeline_def(Primitive2D::Quad), 1);
    const GraphicsPipelineHandle circle = f.context->pipeline_memo.get(*f.context->rhi, f.context->pipelines, f.context->shaders, f.context->back_buffer_format, pipeline_def(Primitive2D::Circle));
    CHECK(f.sink[0].pipeline == quad);
    CHECK(f.sink[0].index_count == 12);
    CHECK(f.sink[1].pipeline == circle);
    CHECK(f.sink[2].pipeline == quad);
    CHECK(f.sink[2].sampler == checker.sampler());
    CHECK(f.sink[3].pipeline == quad);
    CHECK(f.sink[3].sampler == f.context->defaults.sampler);
    CHECK(f.sink[4].index_count == 0);
    CHECK(f.sink[4].vertex_count == 2);
    CHECK(f.sink[5].vertex_count == 3);
    CHECK(batcher.stats().flushes[static_cast<uint32_t>(FlushReason::SamplerChange)] == 1);
    CHECK(batcher.stats().flushes[static_cast<uint32_t>(FlushReason::StreamChange)] == 4);
}

TEST_CASE("BatchRenderer2D: the index limit splits a run of quads")
{
    BatchFixture f;
    BatchRendererDesc desc = f.desc();
    desc.max_indexed_primitives = 2;
    BatchRenderer2D batcher(desc);
    batcher.begin(f.camera);
    for (uint32_t i = 0; i < 5; ++i)
    {
        batcher.draw_rect({}, { 1.0f, 1.0f }, RED);
    }
    batcher.end();
    REQUIRE(f.sink.size() == 3);
    CHECK(f.sink[0].index_count == 12);
    CHECK(f.sink[1].index_count == 12);
    CHECK(f.sink[2].index_count == 6);
    CHECK(batcher.stats().flushes[static_cast<uint32_t>(FlushReason::IndexLimit)] == 2);

    BatchRendererDesc invalid = f.desc();
    invalid.max_indexed_primitives = QUAD_INDEX_MAX_QUADS + 1;
    CHECK_THROWS_AS(BatchRenderer2D{ invalid }, Error);
}

TEST_CASE("BatchRenderer2D: texture slots are shared, and exhaustion flushes (32 and 16 slot permutations)")
{
    for (const uint32_t bindings : { 0u, 16u })
    {
        BatchFixture f;
        BatchRendererDesc desc = f.desc();
        desc.max_texture_bindings = bindings;
        BatchRenderer2D batcher(desc);
        const uint32_t slots = batcher.texture_slot_count();
        CHECK(slots == (bindings == 0 ? TextureArrayPermutations::MAX_TEXTURES : TextureArrayPermutations::DEFAULT_TEXTURES));

        const Texture2D first = f.texture();
        std::vector<Texture2D> textures;
        for (uint32_t i = 0; i < slots + 4; ++i)
        {
            textures.emplace_back(f.texture().texture(), first.sampler());
        }

        batcher.begin(f.camera);
        for (uint32_t pass = 0; pass < 2; ++pass)
        {
            batcher.draw_sprite({}, { 1.0f, 1.0f }, textures[0]);
        }
        CHECK(f.sink.empty());
        for (const Texture2D& texture : textures)
        {
            batcher.draw_sprite({}, { 1.0f, 1.0f }, texture);
        }
        batcher.end();

        REQUIRE(f.sink.size() == 2);
        CHECK(f.sink[0].texture_count == slots);
        CHECK(f.sink[1].texture_count == 6);
        CHECK(batcher.stats().flushes[static_cast<uint32_t>(FlushReason::TextureSlotsFull)] == 1);
    }
}

TEST_CASE("BatchRenderer2D: constants are the camera's column-major matrix")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    Camera2D camera(8.0f, 4.0f);
    camera.set_rotation(0.3f);
    batcher.begin(camera);
    batcher.draw_rect({}, { 1.0f, 1.0f }, RED);
    batcher.end();
    float expected[16];
    camera.to_gpu(expected);
    REQUIRE(f.sink.size() == 1);
    REQUIRE(f.sink[0].constants_size == sizeof(expected));
    CHECK(std::memcmp(f.sink[0].constants, expected, sizeof(expected)) == 0);
}

TEST_CASE("BatchRenderer2D: sprite vertices carry the uv rectangle, tint and rotation")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Texture2D texture = f.texture();
    batcher.begin(f.camera);
    batcher.draw_sprite({ 1.0f, 2.0f }, { 2.0f, 4.0f }, texture, RED, 0.0f, { 0.0f, 0.0f }, { 2.0f, 3.0f });
    batcher.end();
    REQUIRE(f.sink.size() == 1);

    Vertex2DQuad vertices[4];
    std::memcpy(vertices, f.sink[0].vertex_buffers[0]->map() + f.sink[0].vertex_offsets[0], sizeof(vertices));
    CHECK(vertices[0].base.position[0] == doctest::Approx(0.0f));
    CHECK(vertices[0].base.position[1] == doctest::Approx(0.0f));
    CHECK(vertices[2].base.position[0] == doctest::Approx(2.0f));
    CHECK(vertices[2].base.position[1] == doctest::Approx(4.0f));
    CHECK(vertices[0].uv[0] == doctest::Approx(0.0f));
    CHECK(vertices[0].uv[1] == doctest::Approx(3.0f));
    CHECK(vertices[2].uv[0] == doctest::Approx(2.0f));
    CHECK(vertices[2].uv[1] == doctest::Approx(0.0f));
    CHECK(vertices[0].base.colour[1] == doctest::Approx(0.0f));
    CHECK(vertices[0].tex_index == doctest::Approx(1.0f));

    f.sink.clear();
    batcher.begin(f.camera);
    batcher.draw_rect({ 0.0f, 0.0f }, { 2.0f, 2.0f }, RED, 1.5707964f);
    batcher.end();
    std::memcpy(vertices, f.sink[0].vertex_buffers[0]->map() + f.sink[0].vertex_offsets[0], sizeof(vertices));
    CHECK(vertices[0].base.position[0] == doctest::Approx(1.0f).epsilon(0.001));
    CHECK(vertices[0].base.position[1] == doctest::Approx(-1.0f).epsilon(0.001));
}

TEST_CASE("BatchRenderer2D: transient pages grow, keep earlier draws alive and are reused after recycle")
{
    BatchFixture f;
    BatchRendererDesc desc = f.desc();
    desc.page_bytes = 256;
    BatchRenderer2D batcher(desc);

    batcher.begin(f.camera);
    batcher.draw_rect({}, { 1.0f, 1.0f }, RED);
    batcher.draw_circle({}, 1.0f, RED);
    batcher.end();
    REQUIRE(f.sink.size() == 2);
    CHECK(batcher.stats().pages == 2);
    CHECK(f.sink[0].vertex_buffers[0] != f.sink[1].vertex_buffers[0]);
    CHECK(f.sink[0].vertex_buffers[0].get() != nullptr);

    const RHIBufferPtr first_page = f.sink[0].vertex_buffers[0];
    const uint32_t slot = f.context->rhi->frame_slot();
    f.sink.clear();
    batcher.recycle(slot);
    batcher.begin(f.camera);
    batcher.draw_rect({}, { 1.0f, 1.0f }, RED);
    batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_buffers[0] == first_page);
    CHECK(f.sink[0].vertex_offsets[0] == slot * 256);
    CHECK(batcher.stats().pages == 2);
}

TEST_CASE("BatchRenderer2D: a request larger than a page gets its own page")
{
    BatchFixture f;
    BatchRendererDesc desc = f.desc();
    desc.page_bytes = 256;
    BatchRenderer2D batcher(desc);
    batcher.begin(f.camera);
    for (uint32_t i = 0; i < 10; ++i)
    {
        batcher.draw_rect({}, { 1.0f, 1.0f }, RED);
    }
    batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK(batcher.stats().bytes == 1600);
    CHECK(batcher.stats().pages == 1);
}

TEST_CASE("BatchRenderer2D: two scenes in one frame keep separate regions")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    for (uint32_t scene = 0; scene < 2; ++scene)
    {
        batcher.begin(f.camera);
        batcher.draw_rect({}, { 1.0f, 1.0f }, RED);
        batcher.end();
    }
    REQUIRE(f.sink.size() == 2);
    CHECK(f.sink[0].vertex_buffers[0] == f.sink[1].vertex_buffers[0]);
    CHECK(f.sink[1].vertex_offsets[0] >= f.sink[0].vertex_offsets[0] + 160);
}

TEST_CASE("BatchRenderer: a derived class can register its own stream")
{
    BatchFixture f;
    ExtraStreamBatcher batcher(f.desc());
    batcher.begin(f.camera);
    batcher.line();
    batcher.line();
    batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].vertex_count == 4);
    CHECK(f.sink[0].index_buffer.get() == nullptr);
}

TEST_CASE("BatchRenderer: two batchers sharing a sink keep their order")
{
    BatchFixture f;
    BatchRenderer2D first(f.desc());
    BatchRenderer2D second(f.desc());
    first.begin(f.camera);
    second.begin(f.camera);
    first.draw_rect({}, { 1.0f, 1.0f }, RED);
    first.end();
    second.draw_circle({}, 1.0f, RED);
    second.end();
    REQUIRE(f.sink.size() == 2);
    CHECK(f.sink[0].index_count == 6);
    CHECK(f.sink[1].index_count == 6);
    CHECK(f.sink[0].pipeline != f.sink[1].pipeline);
}

TEST_CASE("BatchRenderer: invalid stream descriptions throw")
{
    class Probe : public BatchRenderer
    {
    public:
        using BatchRenderer::BatchRenderer;
        void add(const BatchStreamDesc& desc) { (void)register_stream(desc); }
    };
    BatchFixture f;
    Probe probe(f.desc());
    CHECK_THROWS_AS(probe.add({ 0, 2, 0, false, &pipeline_def(Primitive2D::Line) }), Error);
    CHECK_THROWS_AS(probe.add({ 28, 3, 6, false, &pipeline_def(Primitive2D::Line) }), Error);
    CHECK_NOTHROW(probe.add({ 28, 4, 6, false, &pipeline_def(Primitive2D::Line) }));
}

TEST_CASE("BatchRenderer2D: pipelines are acquired again after release_pipelines")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    batcher.begin(f.camera);
    batcher.draw_circle({}, 1.0f, RED);
    batcher.flush();
    const GraphicsPipelineHandle before = f.sink[0].pipeline;
    release_pipelines(*f.context);
    batcher.draw_circle({}, 1.0f, RED);
    batcher.end();
    REQUIRE(f.sink.size() == 2);
    CHECK_NOTHROW(f.context->pipelines.resolve(f.sink[1].pipeline));
    CHECK_THROWS_AS(f.context->pipelines.resolve(before), Error);
}

namespace
{

BatchTarget target_for(BatchFixture& f, float width, float height)
{
    return { f.sink, { RHIFormat::BGRA8Unorm, RHIFormat::Undefined }, { width, height } };
}

} // namespace

TEST_CASE("BatchRenderer2D: a clip change flushes and the draws carry the scissor")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 50.0f);
    batcher.begin(camera, target_for(f, 200.0f, 100.0f));
    batcher.draw_rect({ 5.0f, 5.0f }, { 2.0f, 2.0f }, RED);
    {
        ClipScope clip(batcher, { 25.0f, 10.0f }, { 50.0f, 20.0f });
        batcher.draw_rect({ 5.0f, 5.0f }, { 2.0f, 2.0f }, RED);
        batcher.draw_rect({ 6.0f, 5.0f }, { 2.0f, 2.0f }, RED);
    }
    batcher.draw_rect({ 5.0f, 5.0f }, { 2.0f, 2.0f }, RED);
    batcher.end();

    REQUIRE(f.sink.size() == 3);
    CHECK_FALSE(f.sink[0].has_scissor);
    REQUIRE(f.sink[1].has_scissor);
    CHECK(f.sink[1].scissor.x == 0);
    CHECK(f.sink[1].scissor.y == 60);
    CHECK(f.sink[1].scissor.width == 100);
    CHECK(f.sink[1].scissor.height == 40);
    CHECK_FALSE(f.sink[2].has_scissor);
    CHECK(batcher.stats().flushes[static_cast<uint32_t>(FlushReason::ScissorChange)] == 2);
}

TEST_CASE("BatchRenderer2D: pushing the same clip again does not flush")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 50.0f);
    batcher.begin(camera, target_for(f, 100.0f, 50.0f));
    {
        ClipScope outer(batcher, { 25.0f, 25.0f }, { 50.0f, 50.0f });
        batcher.draw_rect({ 5.0f, 5.0f }, { 2.0f, 2.0f }, RED);
        ClipScope inner(batcher, { 25.0f, 25.0f }, { 50.0f, 50.0f });
        batcher.draw_rect({ 6.0f, 5.0f }, { 2.0f, 2.0f }, RED);
    }
    batcher.end();
    CHECK(f.sink.size() == 1);
}

TEST_CASE("BatchRenderer2D: nested clips intersect and the outer clip returns on pop")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 100.0f);
    batcher.begin(camera, target_for(f, 100.0f, 100.0f));
    batcher.push_clip({ 30.0f, 70.0f }, { 60.0f, 60.0f });
    batcher.push_clip({ 60.0f, 70.0f }, { 60.0f, 60.0f });
    CHECK(batcher.clip_depth() == 2);
    CHECK(batcher.clip_scissor().x == 30);
    CHECK(batcher.clip_scissor().width == 30);
    batcher.draw_rect({ 40.0f, 70.0f }, { 2.0f, 2.0f }, RED);
    batcher.pop_clip();
    CHECK(batcher.clip_scissor().x == 0);
    CHECK(batcher.clip_scissor().width == 60);
    batcher.draw_rect({ 40.0f, 70.0f }, { 2.0f, 2.0f }, RED);
    batcher.pop_clip();
    batcher.end();

    REQUIRE(f.sink.size() == 2);
    CHECK(f.sink[0].scissor.width == 30);
    CHECK(f.sink[1].scissor.width == 60);
}

TEST_CASE("BatchRenderer2D: an empty clip culls draws without reaching the scissor")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 100.0f);
    batcher.begin(camera, target_for(f, 100.0f, 100.0f));
    batcher.push_clip({ 10.0f, 10.0f }, { 10.0f, 10.0f });
    batcher.push_clip({ 80.0f, 80.0f }, { 10.0f, 10.0f });
    batcher.draw_rect({ 80.0f, 80.0f }, { 4.0f, 4.0f }, RED);
    batcher.draw_circle({ 80.0f, 80.0f }, 2.0f, RED);
    batcher.draw_line({ 0.0f, 0.0f }, { 1.0f, 1.0f }, RED);
    CHECK(batcher.is_clipped({ 10.0f, 10.0f }, { 2.0f, 2.0f }));
    batcher.pop_clip();
    batcher.draw_rect({ 10.0f, 10.0f }, { 4.0f, 4.0f }, RED);
    batcher.pop_clip();
    batcher.end();

    REQUIRE(f.sink.size() == 1);
    CHECK(f.sink[0].scissor.width == 10);
}

TEST_CASE("BatchRenderer2D: a clip is clamped to the framebuffer and rounds outward")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 100.0f);
    batcher.begin(camera, target_for(f, 100.0f, 100.0f));
    batcher.push_clip({ 95.0f, 50.25f }, { 30.0f, 1.0f });
    CHECK(batcher.clip_scissor().x == 80);
    CHECK(batcher.clip_scissor().width == 20);
    CHECK(batcher.clip_scissor().y == 49);
    CHECK(batcher.clip_scissor().height == 2);
    batcher.pop_clip();
    batcher.push_clip({ 500.0f, 500.0f }, { 10.0f, 10.0f });
    CHECK(batcher.is_clipped({ 50.0f, 50.0f }, { 1.0f, 1.0f }));
    batcher.pop_clip();
    batcher.end();
}

TEST_CASE("BatchRenderer2D: is_clipped tests a rect against the active clip")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 100.0f);
    batcher.begin(camera, target_for(f, 100.0f, 100.0f));
    CHECK_FALSE(batcher.is_clipped({ 500.0f, 500.0f }, { 1.0f, 1.0f }));
    {
        ClipScope clip(batcher, { 25.0f, 25.0f }, { 50.0f, 50.0f });
        CHECK_FALSE(batcher.is_clipped({ 10.0f, 10.0f }, { 4.0f, 4.0f }));
        CHECK_FALSE(batcher.is_clipped({ 50.0f, 50.0f }, { 4.0f, 4.0f }));
        CHECK(batcher.is_clipped({ 80.0f, 80.0f }, { 4.0f, 4.0f }));
    }
    batcher.end();
}

TEST_CASE("BatchRenderer2D: clip errors")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 100.0f);
    CHECK_THROWS_AS(batcher.push_clip({ 0.0f, 0.0f }, { 1.0f, 1.0f }), Error);
    batcher.begin(camera);
    CHECK_THROWS_AS(batcher.push_clip({ 0.0f, 0.0f }, { 1.0f, 1.0f }), Error);
    CHECK_THROWS_AS(batcher.pop_clip(), Error);
    batcher.end();
}

TEST_CASE("BatchRenderer2D: ClipScope pops when an exception unwinds")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 100.0f);
    batcher.begin(camera, target_for(f, 100.0f, 100.0f));
    try
    {
        ClipScope clip(batcher, { 50.0f, 50.0f }, { 10.0f, 10.0f });
        throw Error("boom");
    }
    catch (const Error&)
    {
    }
    CHECK(batcher.clip_depth() == 0);
    batcher.end();
}

TEST_CASE("BatchRenderer2D: a scene starts without the previous scene's clip")
{
    BatchFixture f;
    BatchRenderer2D batcher(f.desc());
    const Camera2D camera = Camera2D::screen_space(100.0f, 100.0f);
    batcher.begin(camera, target_for(f, 100.0f, 100.0f));
    batcher.push_clip({ 50.0f, 50.0f }, { 10.0f, 10.0f });
    batcher.draw_rect({ 50.0f, 50.0f }, { 2.0f, 2.0f }, RED);
    batcher.end();
    f.sink.clear();
    batcher.begin(camera, target_for(f, 100.0f, 100.0f));
    CHECK(batcher.clip_depth() == 0);
    batcher.draw_rect({ 50.0f, 50.0f }, { 2.0f, 2.0f }, RED);
    batcher.end();
    REQUIRE(f.sink.size() == 1);
    CHECK_FALSE(f.sink[0].has_scissor);
}
