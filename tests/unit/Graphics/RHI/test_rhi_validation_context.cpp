#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "Oryx/Graphics/RHI/Detail/RHIValidationContext.h"

using namespace oryx;

namespace
{

class CallLog final : public IRHICommandContext
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

constexpr RHIBindingId CONSTANTS = 0;
constexpr RHIBindingId TEXTURES = 1;
constexpr RHIBindingId SAMPLER = 2;

struct Fixture
{
    NullRHI rhi;
    CallLog log;
    RHIValidationContext context{ &log };
    RHITexturePtr texture = rhi.create_texture({ .width = 4, .height = 4, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHIBufferPtr vertices = rhi.create_buffer({ .size = 64, .usage = RHIBufferUsage::Vertex });
    RHIBufferPtr indices = rhi.create_buffer({ .size = 32, .usage = RHIBufferUsage::Index });
    RHISamplerPtr sampler = rhi.create_sampler({});
    RHIGraphicsPipelinePtr pipeline = make_pipeline(true);
    RHIGraphicsPipelinePtr plain = make_pipeline(false);
    RHIRenderPassDesc pass = make_pass();

    RHIRenderPassDesc make_pass()
    {
        RHIRenderPassDesc desc;
        desc.colour[0].target = target.get();
        desc.colour_count = 1;
        return desc;
    }

    RHIGraphicsPipelinePtr make_pipeline(bool two_slots)
    {
        const RHIShaderStageMask both = RHIShaderStageMask::Vertex | RHIShaderStageMask::Pixel;
        const RHIBindingDesc bindings[] = {
            { .kind = RHIBindingKind::Constants, .stage_mask = both, .slot = 0, .size = 16 },
            { .kind = RHIBindingKind::SampledTexture, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0, .array_count = 3 },
            { .kind = RHIBindingKind::Sampler, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0 },
        };
        const RHIVertexAttribute attributes[] = {
            { .location = 0, .format = RHIVertexFormat::Float2, .offset = 0, .slot = 0 },
            { .location = 1, .format = RHIVertexFormat::Float2, .offset = 0, .slot = 1 },
        };
        RHIGraphicsPipelineDesc desc;
        desc.vertex = rhi.create_vertex_shader({ .stage = RHIShaderStage::Vertex });
        desc.pixel = rhi.create_pixel_shader({ .stage = RHIShaderStage::Pixel });
        desc.colour_formats[0] = RHIFormat::RGBA8Unorm;
        desc.bindings = bindings;
        desc.binding_count = static_cast<uint32_t>(std::size(bindings));
        desc.vertex_input.attributes = attributes;
        desc.vertex_input.attribute_count = two_slots ? 2 : 1;
        desc.vertex_input.streams[0].stride = 8;
        desc.vertex_input.streams[1].stride = 8;
        return rhi.create_graphics_pipeline(desc);
    }

    void bind_constants() { const uint8_t bytes[16] = {}; context.set_constants(CONSTANTS, bytes, 16); }
    void bind_textures(uint32_t count = 3) { for (uint32_t i = 0; i < count; ++i) { context.bind_texture(TEXTURES, *texture, i); } }

    void ready(bool both_slots = true)
    {
        context.begin_pass(pass);
        context.set_pipeline(both_slots ? *pipeline : *plain);
        context.set_vertex_buffer(0, *vertices, 0);
        if (both_slots)
        {
            context.set_vertex_buffer(1, *vertices, 0);
        }
        bind_constants();
        bind_textures();
        context.bind_sampler(SAMPLER, *sampler, 0);
    }
};

std::string what_of(const std::function<void()>& call)
{
    try
    {
        call();
    }
    catch (const Error& error)
    {
        return error.what();
    }
    return "";
}

} // namespace

TEST_CASE("RHIGraphicsPipeline::vertex_slot_mask lists the slots its attributes read")
{
    Fixture f;
    CHECK(f.pipeline->vertex_slot_mask() == 0b11);
    CHECK(f.plain->vertex_slot_mask() == 0b01);
}

TEST_CASE("RHIValidationContext forwards valid calls to the inner context")
{
    Fixture f;
    f.ready();
    f.context.draw(3, 1, 0, 0);
    f.context.end_pass();
    CHECK(f.log.calls == std::vector<std::string>{ "begin", "pipeline", "vertex", "vertex", "constants", "bind_texture", "bind_texture", "bind_texture", "bind_sampler", "draw", "end" });
}

TEST_CASE("RHIValidationContext without an inner context only checks")
{
    Fixture f;
    RHIValidationContext bare;
    bare.begin_pass(f.pass);
    CHECK(bare.in_pass());
    CHECK_THROWS_AS(bare.draw(3, 1, 0, 0), Error);
    bare.end_pass();
    CHECK_FALSE(bare.in_pass());
}

TEST_CASE("RHIValidationContext tracks the pass state machine")
{
    Fixture f;
    CHECK_THROWS_AS(f.context.set_pipeline(*f.pipeline), Error);
    CHECK_THROWS_AS(f.context.end_pass(), Error);
    f.context.begin_pass(f.pass);
    CHECK_THROWS_AS(f.context.begin_pass(f.pass), Error);
    CHECK_THROWS_AS(f.context.draw(3, 1, 0, 0), Error);
    CHECK(f.log.calls == std::vector<std::string>{ "begin" });
    f.context.end_pass();
    CHECK_THROWS_AS(f.context.draw(3, 1, 0, 0), Error);
}

TEST_CASE("RHIValidationContext rejects a pipeline whose formats differ from the pass")
{
    Fixture f;
    RHITexturePtr other = f.rhi.create_texture({ .width = 4, .height = 4, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr bgra = f.rhi.create_render_target({ .colour = other });
    RHIRenderPassDesc pass;
    pass.colour[0].target = bgra.get();
    pass.colour_count = 1;
    f.context.begin_pass(pass);
    CHECK_THROWS_AS(f.context.set_pipeline(*f.pipeline), Error);
}

TEST_CASE("RHIValidationContext draw requires every binding of the pipeline")
{
    Fixture f;
    f.context.begin_pass(f.pass);
    f.context.set_pipeline(*f.plain);
    f.context.set_vertex_buffer(0, *f.vertices, 0);

    CHECK(what_of([&] { f.context.draw(3, 1, 0, 0); }).find("binding 0 is not bound") != std::string::npos);
    f.bind_constants();
    CHECK(what_of([&] { f.context.draw(3, 1, 0, 0); }).find("binding 1 is not bound (element 0)") != std::string::npos);
    f.bind_textures(2);
    CHECK(what_of([&] { f.context.draw(3, 1, 0, 0); }).find("binding 1 is not bound (element 2)") != std::string::npos);
    f.bind_textures(3);
    CHECK(what_of([&] { f.context.draw(3, 1, 0, 0); }).find("binding 2 is not bound") != std::string::npos);
    f.context.bind_sampler(SAMPLER, *f.sampler, 0);
    CHECK_NOTHROW(f.context.draw(3, 1, 0, 0));
}

TEST_CASE("RHIValidationContext draw requires every vertex slot the pipeline reads")
{
    Fixture f;
    f.context.begin_pass(f.pass);
    f.context.set_pipeline(*f.pipeline);
    f.context.set_vertex_buffer(0, *f.vertices, 0);
    f.bind_constants();
    f.bind_textures();
    f.context.bind_sampler(SAMPLER, *f.sampler, 0);
    CHECK(what_of([&] { f.context.draw(3, 1, 0, 0); }).find("vertex slot 1") != std::string::npos);
    f.context.set_vertex_buffer(1, *f.vertices, 0);
    CHECK_NOTHROW(f.context.draw(3, 1, 0, 0));
}

TEST_CASE("RHIValidationContext forgets bound resources on set_pipeline and end_pass")
{
    Fixture f;
    f.ready();
    CHECK_NOTHROW(f.context.draw(3, 1, 0, 0));

    f.context.set_pipeline(*f.pipeline);
    CHECK_THROWS_AS(f.context.draw(3, 1, 0, 0), Error);

    f.context.end_pass();
    f.ready();
    CHECK_NOTHROW(f.context.draw(3, 1, 0, 0));
    f.context.end_pass();
    f.context.begin_pass(f.pass);
    f.context.set_pipeline(*f.pipeline);
    CHECK_THROWS_AS(f.context.draw(3, 1, 0, 0), Error);
}

TEST_CASE("RHIValidationContext checks the index range against the bound index buffer")
{
    Fixture f;
    f.ready(false);
    CHECK(what_of([&] { f.context.draw_indexed(3, 1, 0, 0, 0); }).find("without an index buffer") != std::string::npos);

    f.context.set_index_buffer(*f.indices, 8, false);
    CHECK_NOTHROW(f.context.draw_indexed(12, 1, 0, 0, 0));
    CHECK_NOTHROW(f.context.draw_indexed(4, 1, 8, 0, 0));
    CHECK_THROWS_AS(f.context.draw_indexed(13, 1, 0, 0, 0), Error);
    CHECK_THROWS_AS(f.context.draw_indexed(4, 1, 9, 0, 0), Error);
    CHECK_THROWS_AS(f.context.draw_indexed(0xFFFFFFFFu, 1, 0xFFFFFFFFu, 0, 0), Error);

    f.context.set_index_buffer(*f.indices, 0, true);
    CHECK_NOTHROW(f.context.draw_indexed(8, 1, 0, 0, 0));
    CHECK_THROWS_AS(f.context.draw_indexed(9, 1, 0, 0, 0), Error);
}

TEST_CASE("RHIValidationContext clears the index buffer when the pass ends")
{
    Fixture f;
    f.ready(false);
    f.context.set_index_buffer(*f.indices, 0, true);
    f.context.end_pass();
    f.ready(false);
    CHECK_THROWS_AS(f.context.draw_indexed(1, 1, 0, 0, 0), Error);
}

TEST_CASE("RHIValidationContext formats open debug groups as a breadcrumb path in errors")
{
    Fixture f;
    CHECK(f.context.breadcrumb_path().empty());
    f.context.push_debug_group("Frame");
    f.context.begin_pass(f.pass);
    f.context.push_debug_group("BackBuffer Pass");
    f.context.push_debug_group("Draw Batch 2");
    CHECK(f.context.breadcrumb_path() == "Frame > BackBuffer Pass > Draw Batch 2");
    CHECK(f.context.debug_depth() == 3);
    const std::string message = what_of([&] { f.context.draw(3, 1, 0, 0); });
    CHECK(message.find("[Frame > BackBuffer Pass > Draw Batch 2]") != std::string::npos);
    CHECK(message.find("draw") != std::string::npos);

    f.context.pop_debug_group();
    CHECK(f.context.breadcrumb_path() == "Frame > BackBuffer Pass");
    f.context.pop_debug_group();
    f.context.end_pass();
    f.context.pop_debug_group();
    CHECK(f.context.breadcrumb_path().empty());
    CHECK(what_of([&] { f.context.set_pipeline(*f.pipeline); }).find('[') == std::string::npos);
}

TEST_CASE("RHIValidationContext keeps counting groups past the named breadcrumb limit")
{
    Fixture f;
    const uint32_t depth = RHIValidationContext::MAX_BREADCRUMBS + 4;
    for (uint32_t i = 0; i < depth; ++i)
    {
        f.context.push_debug_group("g");
    }
    CHECK(f.context.debug_depth() == depth);
    const std::string path = f.context.breadcrumb_path();
    CHECK(std::count(path.begin(), path.end(), '>') == RHIValidationContext::MAX_BREADCRUMBS - 1);
    for (uint32_t i = 0; i < depth; ++i)
    {
        f.context.pop_debug_group();
    }
    CHECK_THROWS_AS(f.context.pop_debug_group(), Error);
}

TEST_CASE("RHIValidationContext balances debug groups inside and outside passes separately")
{
    Fixture f;
    f.context.push_debug_group("outside");
    f.context.begin_pass(f.pass);
    CHECK_THROWS_AS(f.context.pop_debug_group(), Error);
    f.context.push_debug_group("inside");
    CHECK_THROWS_AS(f.context.end_pass(), Error);
    f.context.pop_debug_group();
    f.context.end_pass();
    f.context.pop_debug_group();
}

TEST_CASE("RHIValidationContext validates resources against the bindings")
{
    Fixture f;
    f.context.begin_pass(f.pass);
    f.context.set_pipeline(*f.plain);
    CHECK_THROWS_AS(f.context.set_vertex_buffer(RHI_MAX_VERTEX_SLOTS, *f.vertices, 0), Error);
    CHECK_THROWS_AS(f.context.set_vertex_buffer(0, *f.indices, 0), Error);
    CHECK_THROWS_AS(f.context.set_vertex_buffer(0, *f.vertices, 65), Error);
    CHECK_THROWS_AS(f.context.set_index_buffer(*f.vertices, 0, true), Error);
    CHECK_THROWS_AS(f.context.bind_texture(TEXTURES, *f.texture, 3), Error);
    CHECK_THROWS_AS(f.context.bind_texture(CONSTANTS, *f.texture, 0), Error);
    CHECK_THROWS_AS(f.context.bind_sampler(TEXTURES, *f.sampler, 0), Error);
    const uint8_t bytes[32] = {};
    CHECK_THROWS_AS(f.context.set_constants(CONSTANTS, bytes, 32), Error);
    CHECK_THROWS_AS(f.context.set_constants(CONSTANTS, nullptr, 16), Error);
    CHECK_THROWS_AS(f.context.set_constants(99, bytes, 16), Error);
}

TEST_CASE("RHIValidationContext wraps a backend context to check a hand-built stream")
{
    Fixture f;
    RHICommandStream stream;
    stream.emplace<RHIBeginPassCommand>(f.pass);
    stream.emplace<RHISetPipelineCommand>(*f.plain);
    stream.emplace<RHIDrawCommand>(3u, 1u, 0u, 0u);
    stream.emplace<RHIEndPassCommand>();

    CallLog backend;
    RHIValidationContext checked(&backend);
    std::string failure;
    uint32_t executed = 0;
    for (const RHICommand& command : stream)
    {
        try
        {
            command.execute(checked);
            ++executed;
        }
        catch (const Error& error)
        {
            failure = error.what();
            break;
        }
    }
    CHECK(executed == 2);
    CHECK(failure.find("in draw") != std::string::npos);
    CHECK(backend.calls == std::vector<std::string>{ "begin", "pipeline" });
}

TEST_CASE("RHICommandList::execute names the failing command")
{
    Fixture f;
    f.ready(false);
    f.context.draw(3, 1, 0, 0);
    f.context.end_pass();

    RHICommandList list;
    list.begin_pass(f.target.get());
    list.set_pipeline(f.plain.get());
    list.end_pass();

    class Failing final : public IRHICommandContext
    {
    public:
        void begin_pass(const RHIRenderPassDesc&) override {}
        void set_pipeline(RHIGraphicsPipeline&) override { throw Error("backend refused the pipeline", "driver detail"); }
        void set_viewport(const RHIViewportState&) override {}
        void set_scissor(const RHIScissorRect&) override {}
        void set_vertex_buffer(uint32_t, RHIBuffer&, uint32_t) override {}
        void set_index_buffer(RHIBuffer&, uint32_t, bool) override {}
        void set_constants(RHIBindingId, const uint8_t*, uint32_t) override {}
        void bind_buffer(RHIBindingId, RHIBuffer&, uint32_t, uint32_t) override {}
        void bind_texture(RHIBindingId, RHITexture&, uint32_t) override {}
        void bind_sampler(RHIBindingId, RHISampler&, uint32_t) override {}
        void draw(uint32_t, uint32_t, uint32_t, uint32_t) override {}
        void draw_indexed(uint32_t, uint32_t, uint32_t, int32_t, uint32_t) override {}
        void push_debug_group(const char*) override {}
        void pop_debug_group() override {}
        void end_pass() override {}
        void copy_buffer(RHIBuffer&, uint32_t, RHIBuffer&, uint32_t, uint32_t) override {}
    } failing;

    try
    {
        list.execute(failing);
        FAIL("execute did not throw");
    }
    catch (const Error& error)
    {
        const std::string message = error.what();
        CHECK(message.find("backend refused the pipeline") != std::string::npos);
        CHECK(message.find("command #1 SetPipeline") != std::string::npos);
        CHECK(error.detail() == "driver detail");
    }
}

TEST_CASE("RHIDebugScope pops on exit and stays quiet while unwinding")
{
    Fixture f;
    RHICommandList list;
    {
        RHIDebugScope scope(list, "outer");
        CHECK(list.debug_depth() == 1);
    }
    CHECK(list.debug_depth() == 0);

    list.begin_pass(f.target.get());
    try
    {
        RHIDebugScope scope(list, "outer");
        list.end_pass();
        throw Error("mid-recording failure");
    }
    catch (const Error&)
    {
    }
    CHECK(list.debug_depth() == 1);
}

TEST_CASE("RHI binding arrays are limited to the per-element bind mask width")
{
    Fixture f;
    const RHIBindingDesc too_wide[] = { { .kind = RHIBindingKind::SampledTexture, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0, .array_count = RHI_MAX_TEXTURE_BINDINGS + 1 } };
    RHIGraphicsPipelineDesc desc;
    desc.vertex = f.rhi.create_vertex_shader({ .stage = RHIShaderStage::Vertex });
    desc.pixel = f.rhi.create_pixel_shader({ .stage = RHIShaderStage::Pixel });
    desc.colour_formats[0] = RHIFormat::RGBA8Unorm;
    desc.bindings = too_wide;
    desc.binding_count = 1;
    CHECK_THROWS_AS(f.rhi.create_graphics_pipeline(desc), Error);
}
