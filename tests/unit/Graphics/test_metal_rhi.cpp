#include "doctest.h"

#include "rhi_contract.h"

#if defined(OX_PLATFORM_MACOS) && defined(OX_ENABLE_GRAPHICS)

using namespace oryx;

namespace
{

UniquePtr<IRHI> try_create_metal()
{
    try
    {
        return create_rhi(RHIBackend::Metal);
    }
    catch (const Error& error)
    {
        MESSAGE("Metal unavailable, skipping: ", error.what());
        return {};
    }
}

void clear_offscreen(IRHI& rhi, RHIFormat format, const Colour& colour, std::vector<uint8_t>& pixels)
{
    RHITexturePtr texture = rhi.create_texture({ .width = 5, .height = 3, .format = format, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHICommandList list;
    list.begin_pass(target.get(), { colour, true });
    list.end_pass();
    rhi.submit(list);
    pixels.assign(5 * 3 * 4, 0);
    rhi.read_texture(*texture, pixels.data(), static_cast<uint32_t>(pixels.size()));
}

} // namespace

TEST_CASE("Metal RHI: contract")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    CHECK(rhi->backend() == RHIBackend::Metal);
    CHECK(rhi->capabilities().frames_in_flight == 3);
    oryx::test::run_rhi_contract(*rhi);
}

TEST_CASE("Metal RHI: offscreen clear is read back exactly")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    const Colour colour{ 1.0f, 0.0f, 51.0f / 255.0f, 128.0f / 255.0f };

    std::vector<uint8_t> pixels;
    clear_offscreen(*rhi, RHIFormat::RGBA8Unorm, colour, pixels);
    for (size_t pixel = 0; pixel < 15; ++pixel)
    {
        CHECK(pixels[pixel * 4 + 0] == 255);
        CHECK(pixels[pixel * 4 + 1] == 0);
        CHECK(pixels[pixel * 4 + 2] == 51);
        CHECK(pixels[pixel * 4 + 3] == 128);
    }

    clear_offscreen(*rhi, RHIFormat::BGRA8Unorm, colour, pixels);
    for (size_t pixel = 0; pixel < 15; ++pixel)
    {
        CHECK(pixels[pixel * 4 + 0] == 51);
        CHECK(pixels[pixel * 4 + 1] == 0);
        CHECK(pixels[pixel * 4 + 2] == 255);
        CHECK(pixels[pixel * 4 + 3] == 128);
    }
}

TEST_CASE("Metal RHI: frames in flight with mid-flight drops leak nothing")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    const size_t baseline = RHIResource::live_count();
    {
        RHIViewportPtr viewport = rhi->create_viewport({ .width = 16, .height = 16 });
        for (uint32_t frame = 0; frame < 24; ++frame)
        {
            RHIBufferPtr buffer = rhi->create_buffer({ .size = 64 });
            RHITexturePtr texture = rhi->create_texture({ .width = 4, .height = 4 });
            RHISamplerPtr sampler = rhi->create_sampler({});
            RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
            REQUIRE(back_buffer);
            RHICommandList list;
            list.begin_pass(back_buffer.get(), { Colour{ 0.2f, 0.4f, 0.6f, 1.0f }, true });
            list.set_vertex_buffer(0, buffer.get());
            list.end_pass();
            rhi->submit(list);
            rhi->present(*viewport);
            rhi->end_frame();
        }
        rhi->wait_idle();
    }
    rhi->wait_idle();
    CHECK(RHIResource::live_count() == baseline);
    CHECK(RHIResource::retired_pending() == 0);
}

namespace
{

constexpr const char* TEST_MSL = R"msl(
#include <metal_stdlib>
using namespace metal;

struct VIn
{
    float3 position [[attribute(0)]];
    float4 colour [[attribute(1)]];
    float tex_index [[attribute(2)]];
};

struct VOut
{
    float4 position [[position]];
    float4 colour;
    float tex_index [[flat]];
};

vertex VOut vs_main(VIn in [[stage_in]])
{
    VOut out;
    out.position = float4(in.position, 1.0);
    out.colour = in.colour;
    out.tex_index = in.tex_index;
    return out;
}

fragment float4 fs_flat(VOut in [[stage_in]], constant float4& tint [[buffer(0)]])
{
    return in.colour * tint;
}

fragment float4 fs_textured(VOut in [[stage_in]], array<texture2d<float>, 4> textures [[texture(0)]], sampler smp [[sampler(0)]])
{
    return textures[int(in.tex_index)].sample(smp, float2(0.5, 0.5));
}
)msl";

constexpr uint32_t TARGET_SIZE = 4;
constexpr uint32_t VERTEX_FLOATS = 8;
constexpr uint32_t VERTEX_STRIDE = VERTEX_FLOATS * 4;

struct TestVertex
{
    float values[VERTEX_FLOATS];
};

static_assert(sizeof(TestVertex) == VERTEX_STRIDE);

const RHIVertexAttribute TEST_ATTRIBUTES[3] = {
    { 0, RHIVertexFormat::Float3, 0, 0 },
    { 1, RHIVertexFormat::Float4, 12, 0 },
    { 2, RHIVertexFormat::Float, 28, 0 },
};

// A full-target triangle at depth z.
void fill_triangle(float* out, float z, const Colour& colour, float tex_index = 0.0f)
{
    const float positions[3][2] = { { -1.0f, -1.0f }, { 3.0f, -1.0f }, { -1.0f, 3.0f } };
    for (uint32_t i = 0; i < 3; ++i)
    {
        float* vertex = out + i * VERTEX_FLOATS;
        vertex[0] = positions[i][0];
        vertex[1] = positions[i][1];
        vertex[2] = z;
        vertex[3] = colour.r;
        vertex[4] = colour.g;
        vertex[5] = colour.b;
        vertex[6] = colour.a;
        vertex[7] = tex_index;
    }
}

RHIShaderDesc shader_desc(RHIShaderStage stage, const char* entry, const char* source = TEST_MSL)
{
    return { .stage = stage, .entry_point = entry, .code = reinterpret_cast<const uint8_t*>(source), .code_size = static_cast<uint32_t>(std::strlen(source)) };
}

struct PipelineOptions
{
    const char* pixel_entry = "fs_flat";
    RHIBlendState blend;
    bool depth = false;
    uint32_t attribute_count = 3;
    uint32_t constants_slot = 0;
    RHIFormat colour_format = RHIFormat::RGBA8Unorm;
};

RHIGraphicsPipelinePtr make_test_pipeline(IRHI& rhi, const PipelineOptions& options)
{
    RHIBindingDesc bindings[2];
    uint32_t binding_count = 0;
    if (std::strcmp(options.pixel_entry, "fs_flat") == 0)
    {
        bindings[binding_count++] = { .kind = RHIBindingKind::Constants, .stage_mask = RHIShaderStageMask::Pixel, .slot = options.constants_slot, .size = 16 };
    }
    else
    {
        bindings[binding_count++] = { .kind = RHIBindingKind::SampledTexture, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0, .array_count = 4 };
        bindings[binding_count++] = { .kind = RHIBindingKind::Sampler, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0 };
    }

    RHIGraphicsPipelineDesc desc;
    desc.vertex = rhi.create_vertex_shader(shader_desc(RHIShaderStage::Vertex, "vs_main"));
    desc.pixel = rhi.create_pixel_shader(shader_desc(RHIShaderStage::Pixel, options.pixel_entry));
    desc.vertex_input.attributes = TEST_ATTRIBUTES;
    desc.vertex_input.attribute_count = options.attribute_count;
    desc.vertex_input.streams[0].stride = VERTEX_STRIDE;
    desc.colour_formats[0] = options.colour_format;
    desc.blend[0] = options.blend;
    if (options.depth)
    {
        desc.depth_format = RHIFormat::Depth32Float;
        desc.depth_stencil.depth_test = true;
        desc.depth_stencil.depth_write = true;
    }
    desc.bindings = bindings;
    desc.binding_count = binding_count;
    return rhi.create_graphics_pipeline(desc);
}

RHIBufferPtr make_vertices(IRHI& rhi, const float* data, uint32_t vertex_count)
{
    return rhi.create_buffer({ .size = vertex_count * VERTEX_STRIDE, .usage = RHIBufferUsage::Vertex, .initial_data = reinterpret_cast<const uint8_t*>(data), .initial_data_size = vertex_count * VERTEX_STRIDE });
}

struct OffscreenTarget
{
    RHITexturePtr texture;
    RHIRenderTargetPtr target;
};

OffscreenTarget make_target(IRHI& rhi)
{
    OffscreenTarget result;
    result.texture = rhi.create_texture({ .width = TARGET_SIZE, .height = TARGET_SIZE, .format = RHIFormat::RGBA8Unorm, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    result.target = rhi.create_render_target({ .colour = result.texture });
    return result;
}

std::vector<uint8_t> read_pixels(IRHI& rhi, RHITexture& texture)
{
    std::vector<uint8_t> pixels(TARGET_SIZE * TARGET_SIZE * 4);
    rhi.read_texture(texture, pixels.data(), static_cast<uint32_t>(pixels.size()));
    return pixels;
}

void check_all_pixels(const std::vector<uint8_t>& pixels, uint8_t r, uint8_t g, uint8_t b, uint8_t a, int32_t tolerance = 1)
{
    for (size_t pixel = 0; pixel < TARGET_SIZE * TARGET_SIZE; ++pixel)
    {
        CHECK(std::abs(static_cast<int32_t>(pixels[pixel * 4 + 0]) - r) <= tolerance);
        CHECK(std::abs(static_cast<int32_t>(pixels[pixel * 4 + 1]) - g) <= tolerance);
        CHECK(std::abs(static_cast<int32_t>(pixels[pixel * 4 + 2]) - b) <= tolerance);
        CHECK(std::abs(static_cast<int32_t>(pixels[pixel * 4 + 3]) - a) <= tolerance);
    }
}

void draw_flat(IRHI& rhi, RHIRenderTarget& target, RHIGraphicsPipeline& pipeline, RHIBuffer& vertices, const float tint[4], const Colour& clear)
{
    RHICommandList list;
    list.begin_pass(&target, { clear, true });
    list.set_pipeline(&pipeline);
    list.set_vertex_buffer(0, &vertices);
    list.set_constants(0, tint, 16);
    list.draw(3);
    list.end_pass();
    rhi.submit(list);
}

} // namespace

TEST_CASE("Metal RHI: flat colour and constants-driven colour")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    RHIGraphicsPipelinePtr pipeline = make_test_pipeline(*rhi, {});
    float triangle[3 * VERTEX_FLOATS];
    fill_triangle(triangle, 0.5f, { 1.0f, 0.0f, 0.0f, 1.0f });
    RHIBufferPtr vertices = make_vertices(*rhi, triangle, 3);

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    OffscreenTarget flat = make_target(*rhi);
    draw_flat(*rhi, *flat.target, *pipeline, *vertices, white, { 0.0f, 0.0f, 0.0f, 0.0f });
    check_all_pixels(read_pixels(*rhi, *flat.texture), 255, 0, 0, 255);

    fill_triangle(triangle, 0.5f, { 1.0f, 1.0f, 1.0f, 1.0f });
    RHIBufferPtr white_vertices = make_vertices(*rhi, triangle, 3);
    const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    OffscreenTarget tinted = make_target(*rhi);
    draw_flat(*rhi, *tinted.target, *pipeline, *white_vertices, green, { 0.0f, 0.0f, 0.0f, 0.0f });
    check_all_pixels(read_pixels(*rhi, *tinted.texture), 0, 255, 0, 255);
}

TEST_CASE("Metal RHI: alpha blending")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    PipelineOptions options;
    options.blend = rhi_blend_alpha();
    RHIGraphicsPipelinePtr pipeline = make_test_pipeline(*rhi, options);
    float triangle[3 * VERTEX_FLOATS];
    fill_triangle(triangle, 0.5f, { 1.0f, 1.0f, 1.0f, 0.5f });
    RHIBufferPtr vertices = make_vertices(*rhi, triangle, 3);

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    OffscreenTarget target = make_target(*rhi);
    draw_flat(*rhi, *target.target, *pipeline, *vertices, white, { 0.0f, 0.0f, 1.0f, 1.0f });
    check_all_pixels(read_pixels(*rhi, *target.texture), 128, 128, 255, 255, 2);
}

TEST_CASE("Metal RHI: depth test keeps the nearer triangle")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    PipelineOptions options;
    options.depth = true;
    RHIGraphicsPipelinePtr pipeline = make_test_pipeline(*rhi, options);
    float near_triangle[3 * VERTEX_FLOATS];
    float far_triangle[3 * VERTEX_FLOATS];
    fill_triangle(near_triangle, 0.2f, { 1.0f, 0.0f, 0.0f, 1.0f });
    fill_triangle(far_triangle, 0.8f, { 0.0f, 1.0f, 0.0f, 1.0f });
    RHIBufferPtr near_vertices = make_vertices(*rhi, near_triangle, 3);
    RHIBufferPtr far_vertices = make_vertices(*rhi, far_triangle, 3);
    RHITexturePtr depth = rhi->create_texture({ .width = TARGET_SIZE, .height = TARGET_SIZE, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::DepthStencil });
    OffscreenTarget target = make_target(*rhi);

    RHIRenderPassDesc pass;
    pass.colour_count = 1;
    pass.colour[0].target = target.target.get();
    pass.depth.texture = depth.get();
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    RHICommandList list;
    list.begin_pass(pass);
    list.set_pipeline(pipeline.get());
    list.set_constants(0, white, 16);
    list.set_vertex_buffer(0, near_vertices.get());
    list.draw(3);
    list.set_vertex_buffer(0, far_vertices.get());
    list.draw(3);
    list.end_pass();
    rhi->submit(list);
    check_all_pixels(read_pixels(*rhi, *target.texture), 255, 0, 0, 255);
}

TEST_CASE("Metal RHI: texture array index picks the bound texture")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    PipelineOptions options;
    options.pixel_entry = "fs_textured";
    RHIGraphicsPipelinePtr pipeline = make_test_pipeline(*rhi, options);
    const uint8_t texels[4][4] = { { 255, 0, 0, 255 }, { 0, 255, 0, 255 }, { 0, 0, 255, 255 }, { 255, 255, 255, 255 } };
    RHITexturePtr textures[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        textures[i] = rhi->create_texture({ .width = 1, .height = 1, .format = RHIFormat::RGBA8Unorm, .initial_data = texels[i], .initial_data_size = 4 });
    }
    RHISamplerPtr sampler = rhi->create_sampler({ .min_filter = RHIFilter::Nearest, .mag_filter = RHIFilter::Nearest });

    for (uint32_t pick = 0; pick < 4; ++pick)
    {
        float triangle[3 * VERTEX_FLOATS];
        fill_triangle(triangle, 0.5f, { 1.0f, 1.0f, 1.0f, 1.0f }, static_cast<float>(pick));
        RHIBufferPtr vertices = make_vertices(*rhi, triangle, 3);
        OffscreenTarget target = make_target(*rhi);
        RHICommandList list;
        list.begin_pass(target.target.get(), { Colour{ 0.0f, 0.0f, 0.0f, 0.0f }, true });
        list.set_pipeline(pipeline.get());
        list.set_vertex_buffer(0, vertices.get());
        for (uint32_t i = 0; i < 4; ++i)
        {
            list.bind_texture(0, textures[i].get(), i);
        }
        list.bind_sampler(1, sampler.get());
        list.draw(3);
        list.end_pass();
        rhi->submit(list);
        check_all_pixels(read_pixels(*rhi, *target.texture), texels[pick][0], texels[pick][1], texels[pick][2], texels[pick][3]);
    }
}

TEST_CASE("Metal RHI: indexed draw honours base_vertex and first_index")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    RHIGraphicsPipelinePtr pipeline = make_test_pipeline(*rhi, {});
    float vertices_data[6 * VERTEX_FLOATS];
    fill_triangle(vertices_data, 0.5f, { 0.0f, 1.0f, 0.0f, 1.0f });
    fill_triangle(vertices_data + 3 * VERTEX_FLOATS, 0.5f, { 1.0f, 0.0f, 0.0f, 1.0f });
    RHIBufferPtr vertices = make_vertices(*rhi, vertices_data, 6);
    const uint16_t indices[6] = { 0, 0, 0, 0, 1, 2 };
    RHIBufferPtr index_buffer = rhi->create_buffer({ .size = sizeof(indices), .usage = RHIBufferUsage::Index, .initial_data = reinterpret_cast<const uint8_t*>(indices), .initial_data_size = sizeof(indices) });

    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    OffscreenTarget target = make_target(*rhi);
    RHICommandList list;
    list.begin_pass(target.target.get(), { Colour{ 0.0f, 0.0f, 0.0f, 0.0f }, true });
    list.set_pipeline(pipeline.get());
    list.set_vertex_buffer(0, vertices.get());
    list.set_index_buffer(index_buffer.get(), 0, false);
    list.set_constants(0, white, 16);
    list.draw_indexed(3, 1, 3, 3, 0);
    list.end_pass();
    rhi->submit(list);
    check_all_pixels(read_pixels(*rhi, *target.texture), 255, 0, 0, 255);
}

TEST_CASE("Metal RHI: dynamic vertex regions are not overwritten while in flight")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    RHIGraphicsPipelinePtr pipeline = make_test_pipeline(*rhi, {});
    VertexLayout layout = { { { 0, RHIVertexFormat::Float3, 0, 0 }, { 1, RHIVertexFormat::Float4, 12, 0 }, { 2, RHIVertexFormat::Float, 28, 0 } }, VERTEX_STRIDE };
    VertexBuffer vertices = VertexBuffer::create(*rhi, layout, 3, BufferMode::Dynamic);

    constexpr uint32_t FRAMES = 8;
    OffscreenTarget targets[FRAMES];
    const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    for (uint32_t frame = 0; frame < FRAMES; ++frame)
    {
        float triangle[3 * VERTEX_FLOATS];
        fill_triangle(triangle, 0.5f, { static_cast<float>(frame) / 10.0f, 0.0f, 1.0f, 1.0f });
        vertices.set_data(rhi->frame_slot(), reinterpret_cast<const TestVertex*>(triangle), 3);
        targets[frame] = make_target(*rhi);
        RHICommandList list;
        list.begin_pass(targets[frame].target.get(), { Colour{ 0.0f, 0.0f, 0.0f, 0.0f }, true });
        list.set_pipeline(pipeline.get());
        list.set_vertex_buffer(0, &vertices.rhi(), vertices.offset(rhi->frame_slot()));
        list.set_constants(0, white, 16);
        list.draw(3);
        list.end_pass();
        rhi->submit(list);
        rhi->end_frame();
    }
    rhi->wait_idle();
    for (uint32_t frame = 0; frame < FRAMES; ++frame)
    {
        check_all_pixels(read_pixels(*rhi, *targets[frame].texture), static_cast<uint8_t>(std::lround(frame * 25.5f)), 0, 255, 255, 1);
    }
}

TEST_CASE("Metal RHI: ten frames of pipelines and draws leak nothing")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    const size_t baseline = RHIResource::live_count();
    {
        RHIViewportPtr viewport = rhi->create_viewport({ .width = 16, .height = 16 });
        for (uint32_t frame = 0; frame < 10; ++frame)
        {
            PipelineOptions options;
            options.colour_format = RHIFormat::BGRA8Unorm;
            RHIGraphicsPipelinePtr pipeline = make_test_pipeline(*rhi, options);
            float triangle[3 * VERTEX_FLOATS];
            fill_triangle(triangle, 0.5f, { 1.0f, 0.5f, 0.0f, 1.0f });
            RHIBufferPtr vertices = make_vertices(*rhi, triangle, 3);
            RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
            REQUIRE(back_buffer);
            RHICommandList list;
            list.begin_pass(back_buffer.get(), { Colour{ 0.0f, 0.0f, 0.0f, 1.0f }, true });
            list.set_pipeline(pipeline.get());
            list.set_vertex_buffer(0, vertices.get());
            const float white[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
            list.set_constants(0, white, 16);
            list.draw(3);
            list.end_pass();
            rhi->submit(list);
            rhi->present(*viewport);
            rhi->end_frame();
        }
        rhi->wait_idle();
    }
    rhi->wait_idle();
    CHECK(RHIResource::live_count() == baseline);
    CHECK(RHIResource::retired_pending() == 0);
}

TEST_CASE("Metal RHI: shader errors keep the MSL diagnostics")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    const char* bad = "vertex float4 broken( { this is not metal }";
    try
    {
        rhi->create_vertex_shader(shader_desc(RHIShaderStage::Vertex, "broken", bad));
        FAIL("expected the shader to fail to compile");
    }
    catch (const Error& error)
    {
        CHECK_FALSE(error.detail().empty());
    }
    CHECK_THROWS_AS(rhi->create_vertex_shader(shader_desc(RHIShaderStage::Vertex, "missing_entry")), Error);
    CHECK_THROWS_AS(rhi->create_vertex_shader(shader_desc(RHIShaderStage::Pixel, "vs_main")), Error);
}

#ifdef OX_DEBUG
TEST_CASE("Metal RHI: a layout that disagrees with the shader is rejected in debug")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    PipelineOptions wrong_slot;
    wrong_slot.constants_slot = 3;
    CHECK_THROWS_AS(make_test_pipeline(*rhi, wrong_slot), Error);

    PipelineOptions missing_attribute;
    missing_attribute.attribute_count = 2;
    CHECK_THROWS_AS(make_test_pipeline(*rhi, missing_attribute), Error);
}
#endif

TEST_CASE("Metal RHI: present copies a source texture into the viewport")
{
    UniquePtr<IRHI> rhi = try_create_metal();
    if (!rhi)
    {
        return;
    }
    RHIViewportPtr viewport = rhi->create_viewport({ .width = 4, .height = 4 });
    RHITexturePtr source = rhi->create_texture({ .width = 4, .height = 4, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget | RHITextureUsage::Sampled });
    RHITexturePtr wrong_size = rhi->create_texture({ .width = 2, .height = 2, .format = RHIFormat::BGRA8Unorm });
    CHECK_THROWS_AS(rhi->present(*viewport, wrong_size.get()), Error);

    RHIRenderTargetPtr target = rhi->create_render_target({ .colour = source });
    RHICommandList list;
    list.begin_pass(target.get(), { Colour{ 0.0f, 1.0f, 0.0f, 1.0f }, true });
    list.end_pass();
    rhi->submit(list);
    CHECK_NOTHROW(rhi->present(*viewport, source.get()));
    rhi->end_frame();
    rhi->wait_idle();
}

#else

TEST_CASE("Metal RHI: not built on this platform")
{
}

#endif
