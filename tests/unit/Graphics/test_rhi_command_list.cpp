#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

struct CommandListFixture
{
    NullRHI rhi;
    RHITexturePtr texture = rhi.create_texture({ .width = 4, .height = 4, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHIBufferPtr vertices = rhi.create_buffer({ .size = 64, .usage = RHIBufferUsage::Vertex | RHIBufferUsage::Uniform });
    RHIBufferPtr indices = rhi.create_buffer({ .size = 64, .usage = RHIBufferUsage::Index });
    RHISamplerPtr sampler = rhi.create_sampler({});
    RHIGraphicsPipelinePtr pipeline = rhi.create_graphics_pipeline({
        .vertex = rhi.create_vertex_shader({ .stage = ShaderStage::Vertex }),
        .pixel = rhi.create_pixel_shader({ .stage = ShaderStage::Pixel }),
        .colour_format = RHIFormat::RGBA8Unorm });
    RHICommandList list;
};

} // namespace

TEST_CASE("RHICommandList records a full pass in order")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get(), { Colour{ 1.0f, 0.0f, 0.0f, 1.0f }, true });
    f.list.set_pipeline(f.pipeline.get());
    f.list.set_vertex_buffer(0, f.vertices.get(), 8);
    f.list.set_index_buffer(f.indices.get(), 4, false);
    f.list.bind(1, f.vertices.get());
    f.list.bind(2, f.texture.get());
    f.list.bind(3, f.sampler.get());
    f.list.draw(3);
    f.list.draw_indexed(6, 2, 1);
    f.list.end_pass();

    std::vector<const RHICommand*> commands;
    for (const RHICommand& command : f.list)
    {
        commands.push_back(&command);
    }
    REQUIRE(commands.size() == 10);
    CHECK(f.list.size() == 10);

    const RHIBeginPassCommand* begin = command_cast<RHIBeginPassCommand>(*commands[0]);
    REQUIRE(begin != nullptr);
    CHECK(&begin->target() == f.target.get());
    CHECK(begin->clear().colour.r == 1.0f);
    CHECK(commands[1]->type() == RHICommandType::SetPipeline);
    CHECK(command_cast<RHISetVertexBufferCommand>(*commands[2])->offset() == 8);
    CHECK_FALSE(command_cast<RHISetIndexBufferCommand>(*commands[3])->index32());
    CHECK(&command_cast<RHIBindBufferCommand>(*commands[4])->buffer() == f.vertices.get());
    CHECK(&command_cast<RHIBindTextureCommand>(*commands[5])->texture() == f.texture.get());
    CHECK(&command_cast<RHIBindSamplerCommand>(*commands[6])->sampler() == f.sampler.get());
    CHECK(command_cast<RHIDrawCommand>(*commands[7])->vertex_count() == 3);
    CHECK(command_cast<RHIDrawIndexedCommand>(*commands[8])->instance_count() == 2);
    CHECK(commands[9]->type() == RHICommandType::EndPass);
    CHECK(command_cast<RHIDrawCommand>(*commands[9]) == nullptr);
    CHECK_FALSE(f.list.in_pass());
}

TEST_CASE("RHICommandList rejects commands outside a pass")
{
    CommandListFixture f;
    CHECK_THROWS_AS(f.list.set_pipeline(f.pipeline.get()), Error);
    CHECK_THROWS_AS(f.list.set_vertex_buffer(0, f.vertices.get()), Error);
    CHECK_THROWS_AS(f.list.set_index_buffer(f.indices.get()), Error);
    CHECK_THROWS_AS(f.list.bind(0, f.vertices.get()), Error);
    CHECK_THROWS_AS(f.list.draw(3), Error);
    CHECK_THROWS_AS(f.list.draw_indexed(3), Error);
    CHECK_THROWS_AS(f.list.end_pass(), Error);
    CHECK(f.list.empty());
}

TEST_CASE("RHICommandList rejects nested passes")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.begin_pass(f.target.get()), Error);
    CHECK(f.list.size() == 1);
}

TEST_CASE("RHICommandList requires a pipeline to draw or bind")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.draw(3), Error);
    CHECK_THROWS_AS(f.list.bind(0, f.vertices.get()), Error);
    f.list.set_pipeline(f.pipeline.get());
    CHECK_THROWS_AS(f.list.draw_indexed(3), Error);
    f.list.set_index_buffer(f.indices.get());
    CHECK_NOTHROW(f.list.draw_indexed(3));
}

TEST_CASE("RHICommandList pipeline state does not leak into the next pass")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    f.list.set_pipeline(f.pipeline.get());
    f.list.end_pass();
    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.draw(3), Error);
}

TEST_CASE("RHICommandList rejects null resources")
{
    CommandListFixture f;
    CHECK_THROWS_AS(f.list.begin_pass(nullptr), Error);
    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.set_pipeline(nullptr), Error);
    CHECK_THROWS_AS(f.list.set_vertex_buffer(0, nullptr), Error);
    CHECK_THROWS_AS(f.list.set_index_buffer(nullptr), Error);
    f.list.set_pipeline(f.pipeline.get());
    CHECK_THROWS_AS(f.list.bind(0, static_cast<RHIBuffer*>(nullptr)), Error);
    CHECK_THROWS_AS(f.list.bind(0, static_cast<RHITexture*>(nullptr)), Error);
    CHECK_THROWS_AS(f.list.bind(0, static_cast<RHISampler*>(nullptr)), Error);
}

TEST_CASE("RHICommandList clear resets commands and state for reuse")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    f.list.set_pipeline(f.pipeline.get());
    f.list.clear();
    CHECK(f.list.empty());
    CHECK_FALSE(f.list.in_pass());
    CHECK_NOTHROW(f.list.begin_pass(f.target.get()));
    CHECK_THROWS_AS(f.list.draw(3), Error);
}

TEST_CASE("RHICommandList moves with its commands")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    RHICommandList moved = std::move(f.list);
    CHECK(moved.size() == 1);
    CHECK(moved.in_pass());
    moved.end_pass();
    CHECK(moved.size() == 2);
}

namespace
{

class RecordingContext final : public IRHICommandContext
{
public:
    void begin_pass(RHIRenderTarget&, const RHIClear&) override { calls.push_back("begin"); }
    void set_pipeline(RHIGraphicsPipeline&) override { calls.push_back("pipeline"); }
    void set_vertex_buffer(uint32_t, RHIBuffer&, uint32_t) override { calls.push_back("vertex"); }
    void set_index_buffer(RHIBuffer&, uint32_t, bool) override { calls.push_back("index"); }
    void bind_buffer(RHIBindingId, RHIBuffer&) override { calls.push_back("bind_buffer"); }
    void bind_texture(RHIBindingId, RHITexture&) override { calls.push_back("bind_texture"); }
    void bind_sampler(RHIBindingId, RHISampler&) override { calls.push_back("bind_sampler"); }
    void draw(uint32_t, uint32_t, uint32_t) override { calls.push_back("draw"); }
    void draw_indexed(uint32_t, uint32_t, uint32_t) override { calls.push_back("draw_indexed"); }
    void end_pass() override { calls.push_back("end"); }

    std::vector<std::string> calls;
};

} // namespace

TEST_CASE("RHICommandList execute replays commands into the context in order")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    f.list.set_pipeline(f.pipeline.get());
    f.list.bind(0, f.sampler.get());
    f.list.draw(3);
    f.list.end_pass();

    RecordingContext context;
    f.list.execute(context);
    CHECK(context.calls == std::vector<std::string>{ "begin", "pipeline", "bind_sampler", "draw", "end" });
}

TEST_CASE("RHICommandList grows past one arena block and is reusable after clear")
{
    CommandListFixture f;
    for (int32_t round = 0; round < 2; ++round)
    {
        f.list.begin_pass(f.target.get());
        f.list.set_pipeline(f.pipeline.get());
        for (int32_t i = 0; i < 5000; ++i)
        {
            f.list.draw(static_cast<uint32_t>(i));
        }
        f.list.end_pass();
        CHECK(f.list.size() == 5003);

        uint32_t expected = 0;
        for (const RHICommand& command : f.list)
        {
            if (const RHIDrawCommand* draw = command_cast<RHIDrawCommand>(command))
            {
                CHECK(draw->vertex_count() == expected++);
            }
        }
        CHECK(expected == 5000);
        f.list.clear();
    }
}

TEST_CASE("RHICommand sizes stay within the deliberate ceiling")
{
    constexpr size_t CEILING = 56;
    static_assert(sizeof(RHIBeginPassCommand) <= CEILING);
    static_assert(sizeof(RHISetPipelineCommand) <= CEILING);
    static_assert(sizeof(RHISetVertexBufferCommand) <= CEILING);
    static_assert(sizeof(RHISetIndexBufferCommand) <= CEILING);
    static_assert(sizeof(RHIBindBufferCommand) <= CEILING);
    static_assert(sizeof(RHIBindTextureCommand) <= CEILING);
    static_assert(sizeof(RHIBindSamplerCommand) <= CEILING);
    static_assert(sizeof(RHIDrawCommand) <= CEILING);
    static_assert(sizeof(RHIDrawIndexedCommand) <= CEILING);
    static_assert(sizeof(RHIEndPassCommand) <= CEILING);
    static_assert(sizeof(RefCounted) <= 32);
    static_assert(sizeof(RHIGraphicsPipeline) <= sizeof(RefCounted) + 8);
}

TEST_CASE("RHICommandList rejects usage, range and format mismatches")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.set_vertex_buffer(0, f.indices.get()), Error);
    CHECK_THROWS_AS(f.list.set_vertex_buffer(RHI_MAX_VERTEX_SLOTS, f.vertices.get()), Error);
    CHECK_THROWS_AS(f.list.set_vertex_buffer(0, f.vertices.get(), 65), Error);
    CHECK_NOTHROW(f.list.set_vertex_buffer(0, f.vertices.get(), 64));
    CHECK_THROWS_AS(f.list.set_index_buffer(f.vertices.get()), Error);
    CHECK_THROWS_AS(f.list.set_index_buffer(f.indices.get(), 65), Error);

    f.list.set_pipeline(f.pipeline.get());
    CHECK_THROWS_AS(f.list.bind(0, f.indices.get()), Error);
    RHITexturePtr target_only = f.rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::RenderTarget });
    CHECK_THROWS_AS(f.list.bind(0, target_only.get()), Error);
    f.list.end_pass();

    RHITexturePtr bgra = f.rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr bgra_target = f.rhi.create_render_target({ .colour = bgra });
    f.list.begin_pass(bgra_target.get());
    CHECK_THROWS_AS(f.list.set_pipeline(f.pipeline.get()), Error);
}

TEST_CASE("RHICommandList retains recorded resources past the caller's references")
{
    CommandListFixture f;
    RHIBuffer* raw = f.vertices.get();
    f.list.begin_pass(f.target.get());
    f.list.set_vertex_buffer(0, f.vertices.get());
    f.list.end_pass();
    CHECK(raw->ref_count() == 2);

    f.vertices.reset();
    CHECK(raw->ref_count() == 1);
    CHECK(raw->size() == 64);

    f.list.clear();
    CHECK(f.list.retained_count() == 0);
}

TEST_CASE("RHICommandList dedupes repeated resources")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    for (int32_t i = 0; i < 1000; ++i)
    {
        f.list.set_pipeline(f.pipeline.get());
        f.list.set_vertex_buffer(0, f.vertices.get());
        f.list.bind(1, f.vertices.get());
        f.list.bind(2, f.sampler.get());
    }
    f.list.end_pass();
    CHECK(f.list.retained_count() <= 5);
    CHECK(f.vertices->ref_count() == 2);
    CHECK(f.pipeline->ref_count() == 2);
    CHECK(f.sampler->ref_count() == 2);
}

TEST_CASE("RHICommandList honours a custom initial block size")
{
    CommandListFixture f;
    RHICommandList small(256);
    small.begin_pass(f.target.get());
    for (uint32_t i = 0; i < 100; ++i)
    {
        small.set_vertex_buffer(0, f.vertices.get(), i % 64);
    }
    small.end_pass();
    CHECK(small.size() == 102);
}
