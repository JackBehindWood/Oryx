#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

constexpr RHIBindingId CONSTANTS = 0;
constexpr RHIBindingId UNIFORMS = 1;
constexpr RHIBindingId TEXTURES = 2;
constexpr RHIBindingId SAMPLER = 3;
constexpr RHIBindingId ARRAY_TEXTURE = 4;
constexpr RHIBindingId DEPTH_TEXTURE = 5;

struct CommandListFixture
{
    NullRHI rhi;
    RHITexturePtr texture = rhi.create_texture({ .width = 4, .height = 4, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHIBufferPtr vertices = rhi.create_buffer({ .size = 64, .usage = RHIBufferUsage::Vertex | RHIBufferUsage::Uniform });
    RHIBufferPtr indices = rhi.create_buffer({ .size = 64, .usage = RHIBufferUsage::Index });
    RHIBufferPtr large = rhi.create_buffer({ .size = 8192, .usage = RHIBufferUsage::Uniform });
    RHISamplerPtr sampler = rhi.create_sampler({});
    RHITexturePtr array_texture = rhi.create_texture({ .width = 4, .height = 4, .dimension = RHITextureDimension::Tex2DArray, .array_layers = 2 });
    RHITexturePtr depth_sampled = rhi.create_texture({ .width = 4, .height = 4, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::Sampled | RHITextureUsage::DepthStencil });
    RHIGraphicsPipelinePtr pipeline = make_graphics_pipeline(RHIFormat::RGBA8Unorm, RHIFormat::Undefined);
    RHICommandList list;

    RHIGraphicsPipelinePtr make_graphics_pipeline(RHIFormat colour, RHIFormat depth)
    {
        const RHIShaderStageMask both = RHIShaderStageMask::Vertex | RHIShaderStageMask::Pixel;
        const RHIBindingDesc bindings[] = {
            { .kind = RHIBindingKind::Constants, .stage_mask = both, .slot = 0, .size = 64 },
            { .kind = RHIBindingKind::UniformBuffer, .stage_mask = both, .slot = 1, .size = 64 },
            { .kind = RHIBindingKind::SampledTexture, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0, .array_count = 4 },
            { .kind = RHIBindingKind::Sampler, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0 },
            { .kind = RHIBindingKind::SampledTexture, .stage_mask = RHIShaderStageMask::Pixel, .texture_dimension = RHITextureDimension::Tex2DArray, .slot = 4 },
            { .kind = RHIBindingKind::SampledTexture, .stage_mask = RHIShaderStageMask::Pixel, .data_type = RHIDataType::Depth, .slot = 5 },
        };
        RHIGraphicsPipelineDesc desc;
        desc.vertex = rhi.create_vertex_shader({ .stage = RHIShaderStage::Vertex });
        desc.pixel = rhi.create_pixel_shader({ .stage = RHIShaderStage::Pixel });
        desc.colour_formats[0] = colour;
        desc.depth_format = depth;
        desc.bindings = bindings;
        desc.binding_count = static_cast<uint32_t>(std::size(bindings));
        return rhi.create_graphics_pipeline(desc);
    }

    void begin_with_pipeline()
    {
        list.begin_pass(target.get());
        list.set_pipeline(pipeline.get());
    }

    static constexpr size_t BIND_ALL_COMMANDS = 9;

    void bind_all()
    {
        const std::array<float, 16> constants = {};
        list.set_constants(CONSTANTS, constants.data(), sizeof(constants));
        list.bind_buffer(UNIFORMS, vertices.get(), 0, 32);
        for (uint32_t i = 0; i < 4; ++i)
        {
            list.bind_texture(TEXTURES, texture.get(), i);
        }
        list.bind_sampler(SAMPLER, sampler.get());
        list.bind_texture(ARRAY_TEXTURE, array_texture.get());
        list.bind_texture(DEPTH_TEXTURE, depth_sampled.get());
    }

    void begin_ready()
    {
        begin_with_pipeline();
        bind_all();
    }
};

} // namespace

TEST_CASE("RHICommandList records a full pass in order")
{
    CommandListFixture f;
    const std::array<float, 16> constants = {};
    f.list.begin_pass(f.target.get(), { Colour{ 1.0f, 0.0f, 0.0f, 1.0f }, true });
    f.list.set_pipeline(f.pipeline.get());
    f.list.set_viewport({ 0.0f, 0.0f, 4.0f, 4.0f });
    f.list.set_scissor({ 1, 1, 2, 2 });
    f.list.set_vertex_buffer(0, f.vertices.get(), 8);
    f.list.set_index_buffer(f.indices.get(), 4, false);
    f.list.set_constants(CONSTANTS, constants.data(), sizeof(constants));
    f.list.bind_buffer(UNIFORMS, f.vertices.get(), 8, 32);
    f.list.bind_texture(TEXTURES, f.texture.get(), 3);
    f.list.bind_sampler(SAMPLER, f.sampler.get());
    for (uint32_t i = 0; i < 3; ++i)
    {
        f.list.bind_texture(TEXTURES, f.texture.get(), i);
    }
    f.list.bind_texture(ARRAY_TEXTURE, f.array_texture.get());
    f.list.bind_texture(DEPTH_TEXTURE, f.depth_sampled.get());
    f.list.push_debug_group("group");
    f.list.draw(3, 2, 1, 5);
    f.list.draw_indexed(6, 2, 1, -4, 7);
    f.list.pop_debug_group();
    f.list.end_pass();

    std::vector<const RHICommand*> commands;
    for (const RHICommand& command : f.list)
    {
        commands.push_back(&command);
    }
    REQUIRE(commands.size() == 20);
    CHECK(f.list.size() == 20);

    const RHIBeginPassCommand* begin = command_cast<RHIBeginPassCommand>(*commands[0]);
    REQUIRE(begin != nullptr);
    CHECK(begin->pass().colour_count == 1);
    CHECK(begin->pass().colour[0].target == f.target.get());
    CHECK(begin->pass().colour[0].load == RHILoadAction::Clear);
    CHECK(begin->pass().colour[0].clear_colour.r == 1.0f);
    CHECK(std::string_view(commands[1]->command_name()) == "SetPipeline");
    CHECK(command_cast<RHISetViewportCommand>(*commands[2])->viewport().width == 4.0f);
    CHECK(command_cast<RHISetScissorCommand>(*commands[3])->scissor().x == 1);
    CHECK(command_cast<RHISetVertexBufferCommand>(*commands[4])->offset() == 8);
    CHECK_FALSE(command_cast<RHISetIndexBufferCommand>(*commands[5])->index32());
    const RHISetConstantsCommand* set_constants = command_cast<RHISetConstantsCommand>(*commands[6]);
    CHECK(set_constants->binding() == CONSTANTS);
    CHECK(set_constants->size() == sizeof(constants));
    const RHIBindBufferCommand* bind_buffer = command_cast<RHIBindBufferCommand>(*commands[7]);
    CHECK(&bind_buffer->buffer() == f.vertices.get());
    CHECK(bind_buffer->offset() == 8);
    CHECK(bind_buffer->size() == 32);
    const RHIBindTextureCommand* bind_texture = command_cast<RHIBindTextureCommand>(*commands[8]);
    CHECK(&bind_texture->texture() == f.texture.get());
    CHECK(bind_texture->array_index() == 3);
    CHECK(&command_cast<RHIBindSamplerCommand>(*commands[9])->sampler() == f.sampler.get());
    CHECK(std::string(command_cast<RHIPushDebugGroupCommand>(*commands[15])->name()) == "group");
    const RHIDrawCommand* draw = command_cast<RHIDrawCommand>(*commands[16]);
    CHECK(draw->vertex_count() == 3);
    CHECK(draw->instance_count() == 2);
    CHECK(draw->first_vertex() == 1);
    CHECK(draw->first_instance() == 5);
    const RHIDrawIndexedCommand* indexed = command_cast<RHIDrawIndexedCommand>(*commands[17]);
    CHECK(indexed->instance_count() == 2);
    CHECK(indexed->base_vertex() == -4);
    CHECK(indexed->first_instance() == 7);
    CHECK(std::string_view(commands[18]->command_name()) == "PopDebugGroup");
    CHECK(std::string_view(commands[19]->command_name()) == "EndPass");
    CHECK(command_cast<RHIDrawCommand>(*commands[19]) == nullptr);
    CHECK_FALSE(f.list.in_pass());
}

TEST_CASE("RHICommandList rejects commands outside a pass")
{
    CommandListFixture f;
    CHECK_THROWS_AS(f.list.set_pipeline(f.pipeline.get()), Error);
    CHECK_THROWS_AS(f.list.set_viewport({ 0.0f, 0.0f, 1.0f, 1.0f }), Error);
    CHECK_THROWS_AS(f.list.set_scissor({ 0, 0, 1, 1 }), Error);
    CHECK_THROWS_AS(f.list.set_vertex_buffer(0, f.vertices.get()), Error);
    CHECK_THROWS_AS(f.list.set_index_buffer(f.indices.get()), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, f.vertices.get(), 0, 16), Error);
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
    const std::array<float, 16> constants = {};
    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.draw(3), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, f.vertices.get(), 0, 16), Error);
    CHECK_THROWS_AS(f.list.set_constants(CONSTANTS, constants.data(), 16), Error);
    f.list.set_pipeline(f.pipeline.get());
    CHECK_THROWS_AS(f.list.draw_indexed(3), Error);
    f.list.set_index_buffer(f.indices.get());
    CHECK_THROWS_AS(f.list.draw_indexed(3), Error);
    f.bind_all();
    CHECK_NOTHROW(f.list.draw_indexed(3));
}

TEST_CASE("RHICommandList pipeline state does not leak into the next pass")
{
    CommandListFixture f;
    f.begin_with_pipeline();
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
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, nullptr, 0, 16), Error);
    CHECK_THROWS_AS(f.list.bind_texture(TEXTURES, nullptr), Error);
    CHECK_THROWS_AS(f.list.bind_sampler(SAMPLER, nullptr), Error);
    CHECK_THROWS_AS(f.list.set_constants(CONSTANTS, nullptr, 16), Error);
    CHECK_THROWS_AS(f.list.push_debug_group(nullptr), Error);
}

TEST_CASE("RHICommandList clear resets commands and state for reuse")
{
    CommandListFixture f;
    f.begin_with_pipeline();
    f.list.push_debug_group("open");
    f.list.clear();
    CHECK(f.list.empty());
    CHECK_FALSE(f.list.in_pass());
    CHECK(f.list.debug_depth() == 0);
    CHECK_NOTHROW(f.list.begin_pass(f.target.get()));
    CHECK_THROWS_AS(f.list.draw(3), Error);
}

TEST_CASE("RHICommandList moves with its commands and state")
{
    CommandListFixture f;
    f.begin_ready();
    const size_t ready = 2 + CommandListFixture::BIND_ALL_COMMANDS;
    RHICommandList moved = std::move(f.list);
    CHECK(moved.size() == ready);
    CHECK(moved.in_pass());
    CHECK_NOTHROW(moved.draw(3));
    moved.end_pass();
    CHECK(moved.size() == ready + 2);

    CHECK(f.list.empty());
    CHECK_FALSE(f.list.in_pass());

    RHICommandList assigned;
    assigned = std::move(moved);
    CHECK(assigned.size() == ready + 2);
    CHECK(moved.empty());
}

namespace
{

class RecordingContext final : public IRHICommandContext
{
public:
    void begin_pass(const RHIRenderPassDesc&) override { calls.push_back("begin"); }
    void set_pipeline(RHIGraphicsPipeline&) override { calls.push_back("pipeline"); }
    void set_viewport(const RHIViewportState&) override { calls.push_back("viewport"); }
    void set_scissor(const RHIScissorRect&) override { calls.push_back("scissor"); }
    void set_vertex_buffer(uint32_t, RHIBuffer&, uint32_t) override { calls.push_back("vertex"); }
    void set_index_buffer(RHIBuffer&, uint32_t, bool) override { calls.push_back("index"); }
    void set_constants(RHIBindingId, const uint8_t*, uint32_t) override { calls.push_back("constants"); }
    void bind_buffer(RHIBindingId, RHIBuffer&, uint32_t, uint32_t) override { calls.push_back("bind_buffer"); }
    void bind_texture(RHIBindingId, RHITexture&, uint32_t) override { calls.push_back("bind_texture"); }
    void bind_sampler(RHIBindingId, RHISampler&, uint32_t) override { calls.push_back("bind_sampler"); }
    void draw(uint32_t, uint32_t, uint32_t, uint32_t) override { calls.push_back("draw"); }
    void draw_indexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override { calls.push_back("draw_indexed"); }
    void push_debug_group(const char*) override { calls.push_back("push"); }
    void pop_debug_group() override { calls.push_back("pop"); }
    void end_pass() override { calls.push_back("end"); }
    void copy_buffer(RHIBuffer&, uint32_t, RHIBuffer&, uint32_t, uint32_t) override { calls.push_back("copy"); }

    std::vector<std::string> calls;
};

} // namespace

TEST_CASE("RHICommandList execute replays commands into the context in order")
{
    CommandListFixture f;
    f.begin_ready();
    f.list.push_debug_group("g");
    f.list.draw(3);
    f.list.pop_debug_group();
    f.list.end_pass();

    RecordingContext context;
    f.list.execute(context);
    CHECK(context.calls == std::vector<std::string>{ "begin", "pipeline", "constants", "bind_buffer", "bind_texture", "bind_texture", "bind_texture", "bind_texture", "bind_sampler", "bind_texture", "bind_texture", "push", "draw", "pop", "end" });
}

TEST_CASE("RHICommandList grows past one arena block and is reusable after clear")
{
    CommandListFixture f;
    for (int32_t round = 0; round < 2; ++round)
    {
        f.begin_ready();
        for (int32_t i = 0; i < 5000; ++i)
        {
            f.list.draw(static_cast<uint32_t>(i));
        }
        f.list.end_pass();
        CHECK(f.list.size() == 5003 + CommandListFixture::BIND_ALL_COMMANDS);

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
    static_assert(sizeof(RHIBeginPassCommand) <= 512);
    static_assert(sizeof(RHISetPipelineCommand) <= CEILING);
    static_assert(sizeof(RHISetViewportCommand) <= CEILING);
    static_assert(sizeof(RHISetScissorCommand) <= CEILING);
    static_assert(sizeof(RHISetVertexBufferCommand) <= CEILING);
    static_assert(sizeof(RHISetIndexBufferCommand) <= CEILING);
    static_assert(sizeof(RHICopyBufferCommand) <= CEILING);
    static_assert(sizeof(RHISetConstantsCommand) <= CEILING);
    static_assert(sizeof(RHIBindBufferCommand) <= CEILING);
    static_assert(sizeof(RHIBindTextureCommand) <= CEILING);
    static_assert(sizeof(RHIBindSamplerCommand) <= CEILING);
    static_assert(sizeof(RHIDrawCommand) <= CEILING);
    static_assert(sizeof(RHIDrawIndexedCommand) <= CEILING);
    static_assert(sizeof(RHIPushDebugGroupCommand) <= CEILING);
    static_assert(sizeof(RHIPopDebugGroupCommand) <= CEILING);
    static_assert(sizeof(RHIEndPassCommand) <= CEILING);
    static_assert(sizeof(RefCounted) <= 32);
    static_assert(sizeof(RHIGraphicsPipeline) <= 1024);
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
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, f.indices.get(), 0, 16), Error);
    RHITexturePtr target_only = f.rhi.create_texture({ .width = 4, .height = 4, .usage = RHITextureUsage::RenderTarget });
    CHECK_THROWS_AS(f.list.bind_texture(TEXTURES, target_only.get()), Error);
    f.list.end_pass();

    RHITexturePtr bgra = f.rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr bgra_target = f.rhi.create_render_target({ .colour = bgra });
    f.list.begin_pass(bgra_target.get());
    CHECK_THROWS_AS(f.list.set_pipeline(f.pipeline.get()), Error);
}

TEST_CASE("RHICommandList validates binding ids and kinds")
{
    CommandListFixture f;
    const std::array<float, 16> constants = {};
    f.begin_with_pipeline();
    CHECK_THROWS_AS(f.list.bind_buffer(99, f.vertices.get(), 0, 16), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(RHI_INVALID_BINDING, f.vertices.get(), 0, 16), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(CONSTANTS, f.vertices.get(), 0, 16), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(TEXTURES, f.vertices.get(), 0, 16), Error);
    CHECK_THROWS_AS(f.list.bind_texture(SAMPLER, f.texture.get()), Error);
    CHECK_THROWS_AS(f.list.bind_texture(UNIFORMS, f.texture.get()), Error);
    CHECK_THROWS_AS(f.list.bind_sampler(TEXTURES, f.sampler.get()), Error);
    CHECK_THROWS_AS(f.list.set_constants(UNIFORMS, constants.data(), 16), Error);
    CHECK_THROWS_AS(f.list.set_constants(99, constants.data(), 16), Error);
}

TEST_CASE("RHICommandList validates buffer binding ranges")
{
    CommandListFixture f;
    f.begin_with_pipeline();
    CHECK_NOTHROW(f.list.bind_buffer(UNIFORMS, f.vertices.get(), 0, 64));
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, f.vertices.get(), 0, 0), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, f.vertices.get(), 32, 64), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, f.vertices.get(), 0xFFFFFFF0u, 64), Error);
    CHECK_THROWS_AS(f.list.bind_buffer(UNIFORMS, f.large.get(), 0, 128), Error);
}

TEST_CASE("RHICommandList validates texture and sampler bindings")
{
    CommandListFixture f;
    f.begin_with_pipeline();
    CHECK_NOTHROW(f.list.bind_texture(TEXTURES, f.texture.get(), 0));
    CHECK_NOTHROW(f.list.bind_texture(TEXTURES, f.texture.get(), 3));
    CHECK_THROWS_AS(f.list.bind_texture(TEXTURES, f.texture.get(), 4), Error);
    CHECK_THROWS_AS(f.list.bind_texture(TEXTURES, f.array_texture.get()), Error);
    CHECK_NOTHROW(f.list.bind_texture(ARRAY_TEXTURE, f.array_texture.get()));
    CHECK_THROWS_AS(f.list.bind_texture(ARRAY_TEXTURE, f.texture.get()), Error);
    CHECK_THROWS_AS(f.list.bind_texture(TEXTURES, f.depth_sampled.get()), Error);
    CHECK_NOTHROW(f.list.bind_texture(DEPTH_TEXTURE, f.depth_sampled.get()));
    CHECK_THROWS_AS(f.list.bind_texture(DEPTH_TEXTURE, f.texture.get()), Error);
    CHECK_NOTHROW(f.list.bind_sampler(SAMPLER, f.sampler.get(), 0));
    CHECK_THROWS_AS(f.list.bind_sampler(SAMPLER, f.sampler.get(), 1), Error);
}

TEST_CASE("RHICommandList enforces the constants limits")
{
    CommandListFixture f;
    std::vector<uint8_t> bytes(8192);
    f.begin_with_pipeline();
    CHECK_NOTHROW(f.list.set_constants(CONSTANTS, bytes.data(), 64));
    CHECK_NOTHROW(f.list.set_constants(CONSTANTS, bytes.data(), 16));
    CHECK_THROWS_AS(f.list.set_constants(CONSTANTS, bytes.data(), 65), Error);
    CHECK_THROWS_AS(f.list.set_constants(CONSTANTS, bytes.data(), 0), Error);
    CHECK_THROWS_AS(f.list.set_constants(CONSTANTS, bytes.data(), RHI_MAX_CONSTANTS_SIZE + 1), Error);
}

TEST_CASE("RHICommandList copies constants into the list")
{
    CommandListFixture f;
    std::array<uint8_t, 8> bytes = { 1, 2, 3, 4, 5, 6, 7, 8 };
    f.begin_with_pipeline();
    f.list.set_constants(CONSTANTS, bytes.data(), static_cast<uint32_t>(bytes.size()));
    bytes.fill(0);

    const RHISetConstantsCommand* command = nullptr;
    for (const RHICommand& recorded : f.list)
    {
        if (const RHISetConstantsCommand* candidate = command_cast<RHISetConstantsCommand>(recorded))
        {
            command = candidate;
        }
    }
    REQUIRE(command != nullptr);
    CHECK(command->data()[0] == 1);
    CHECK(command->data()[7] == 8);
}

TEST_CASE("RHICommandList validates viewport and scissor")
{
    CommandListFixture f;
    f.list.begin_pass(f.target.get());
    CHECK_NOTHROW(f.list.set_viewport({ 0.0f, 0.0f, 4.0f, 4.0f, 0.0f, 1.0f }));
    CHECK_THROWS_AS(f.list.set_viewport({ 0.0f, 0.0f, 0.0f, 4.0f }), Error);
    CHECK_THROWS_AS(f.list.set_viewport({ 0.0f, 0.0f, 4.0f, 4.0f, 0.5f, 0.25f }), Error);
    CHECK_NOTHROW(f.list.set_scissor({ 0, 0, 4, 4 }));
    CHECK_THROWS_AS(f.list.set_scissor({ -1, 0, 2, 2 }), Error);
    CHECK_THROWS_AS(f.list.set_scissor({ 3, 0, 2, 2 }), Error);
}

TEST_CASE("RHICommandList validates pass descriptions")
{
    CommandListFixture f;
    RHITexturePtr second_texture = f.rhi.create_texture({ .width = 4, .height = 4, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr second = f.rhi.create_render_target({ .colour = second_texture });
    RHITexturePtr small_texture = f.rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr small = f.rhi.create_render_target({ .colour = small_texture });
    RHITexturePtr depth = f.rhi.create_texture({ .width = 4, .height = 4, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::DepthStencil });
    RHITexturePtr plain_depth = f.rhi.create_texture({ .width = 4, .height = 4, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::Sampled });

    RHIRenderPassDesc none;
    CHECK_THROWS_AS(f.list.begin_pass(none), Error);

    RHIRenderPassDesc mrt;
    mrt.colour[0].target = f.target.get();
    mrt.colour[1].target = second.get();
    mrt.colour_count = 2;
    mrt.depth.texture = depth.get();
    CHECK_NOTHROW(f.list.begin_pass(mrt));
    f.list.end_pass();

    RHIRenderPassDesc depth_only;
    depth_only.depth.texture = depth.get();
    CHECK_NOTHROW(f.list.begin_pass(depth_only));
    f.list.end_pass();

    RHIRenderPassDesc mismatched = mrt;
    mismatched.colour[1].target = small.get();
    CHECK_THROWS_AS(f.list.begin_pass(mismatched), Error);

    RHIRenderPassDesc missing = mrt;
    missing.colour[1].target = nullptr;
    CHECK_THROWS_AS(f.list.begin_pass(missing), Error);

    RHIRenderPassDesc bad_depth = mrt;
    bad_depth.depth.texture = plain_depth.get();
    CHECK_THROWS_AS(f.list.begin_pass(bad_depth), Error);

    RHIRenderPassDesc too_many = mrt;
    too_many.colour_count = RHI_MAX_COLOUR_TARGETS + 1;
    CHECK_THROWS_AS(f.list.begin_pass(too_many), Error);
    CHECK_FALSE(f.list.in_pass());
}

TEST_CASE("RHICommandList matches pipelines to pass attachments")
{
    CommandListFixture f;
    RHITexturePtr depth = f.rhi.create_texture({ .width = 4, .height = 4, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::DepthStencil });
    RHIGraphicsPipelinePtr with_depth = f.make_graphics_pipeline(RHIFormat::RGBA8Unorm, RHIFormat::Depth32Float);

    RHIRenderPassDesc pass;
    pass.colour[0].target = f.target.get();
    pass.colour_count = 1;
    f.list.begin_pass(pass);
    CHECK_THROWS_AS(f.list.set_pipeline(with_depth.get()), Error);
    CHECK_NOTHROW(f.list.set_pipeline(f.pipeline.get()));
    f.list.end_pass();

    pass.depth.texture = depth.get();
    f.list.begin_pass(pass);
    CHECK_THROWS_AS(f.list.set_pipeline(f.pipeline.get()), Error);
    CHECK_NOTHROW(f.list.set_pipeline(with_depth.get()));
}

TEST_CASE("RHICommandList rejects a pipeline whose sample count differs from the pass")
{
    CommandListFixture f;
    const RHIBindingDesc binding = { .kind = RHIBindingKind::Sampler, .stage_mask = RHIShaderStageMask::Pixel };
    RHIGraphicsPipelineDesc desc;
    desc.vertex = f.rhi.create_vertex_shader({ .stage = RHIShaderStage::Vertex });
    desc.pixel = f.rhi.create_pixel_shader({ .stage = RHIShaderStage::Pixel });
    desc.colour_formats[0] = RHIFormat::RGBA8Unorm;
    desc.sample_count = 4;
    desc.bindings = &binding;
    desc.binding_count = 1;
    RHIGraphicsPipelinePtr multisampled = f.rhi.create_graphics_pipeline(desc);
    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.set_pipeline(multisampled.get()), Error);
}

TEST_CASE("RHICommandList balances debug groups inside and outside passes")
{
    CommandListFixture f;
    CHECK_THROWS_AS(f.list.pop_debug_group(), Error);
    f.list.push_debug_group("frame");
    CHECK(f.list.debug_depth() == 1);

    f.list.begin_pass(f.target.get());
    CHECK_THROWS_AS(f.list.pop_debug_group(), Error);
    f.list.push_debug_group("pass");
    CHECK_THROWS_AS(f.list.end_pass(), Error);
    f.list.pop_debug_group();
    CHECK_NOTHROW(f.list.end_pass());

    CHECK_NOTHROW(f.list.pop_debug_group());
    CHECK(f.list.debug_depth() == 0);
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
        f.list.bind_buffer(UNIFORMS, f.vertices.get(), 0, 16);
        f.list.bind_sampler(SAMPLER, f.sampler.get());
    }
    f.list.end_pass();
    CHECK(f.list.retained_count() <= 5);
    CHECK(f.vertices->ref_count() == 2);
    CHECK(f.pipeline->ref_count() == 2);
    CHECK(f.sampler->ref_count() == 2);
}

TEST_CASE("RHICommandList dedupes a texture array bound element by element")
{
    CommandListFixture f;
    f.begin_with_pipeline();
    RHITexturePtr other = f.rhi.create_texture({ .width = 4, .height = 4 });
    for (int32_t round = 0; round < 100; ++round)
    {
        f.list.bind_texture(TEXTURES, f.texture.get(), 0);
        f.list.bind_texture(TEXTURES, other.get(), 1);
    }
    f.list.end_pass();
    CHECK(f.list.retained_count() <= 5);
    CHECK(f.texture->ref_count() == 3);
    CHECK(other->ref_count() == 2);
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
