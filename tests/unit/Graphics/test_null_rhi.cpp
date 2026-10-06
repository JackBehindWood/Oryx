#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"

using namespace oryx;

namespace
{

RHIViewportDesc viewport_desc()
{
    RHIViewportDesc desc;
    desc.width = 8;
    desc.height = 8;
    return desc;
}

} // namespace

TEST_CASE("NullRHI exposes the last submission and submit count")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    RHIBufferPtr buffer = rhi.create_buffer({ .size = 32 });

    CHECK(rhi.submit_count() == 0);
    CHECK(rhi.last_submission().empty());

    RHICommandList list;
    list.begin_pass(back_buffer.get());
    list.set_vertex_buffer(0, buffer.get());
    list.end_pass();
    rhi.submit(list);
    rhi.present(viewport.get());
    rhi.end_frame();

    CHECK(rhi.submit_count() == 1);
    REQUIRE(rhi.last_submission().size() == 3);
    CHECK(rhi.last_submission()[0] == "BeginPass");
    CHECK(rhi.last_submission()[1] == "SetVertexBuffer");
    CHECK(rhi.last_submission()[2] == "EndPass");

    list.clear();
    list.begin_pass(back_buffer.get());
    list.end_pass();
    rhi.submit(list);
    CHECK(rhi.submit_count() == 2);
    CHECK(rhi.last_submission().size() == 2);
}

TEST_CASE("NullRHI submit rejects an unfinished pass and records nothing")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();

    RHICommandList list;
    list.begin_pass(back_buffer.get());
    CHECK_THROWS_AS(rhi.submit(list), Error);
    CHECK(rhi.submit_count() == 0);
}

TEST_CASE("NullRHI retains submitted resources until end_frame")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    RHIBufferPtr buffer = rhi.create_buffer({ .size = 16 });
    const NullBuffer* raw = static_cast<NullBuffer*>(buffer.get());

    RHICommandList list;
    list.begin_pass(back_buffer.get());
    list.set_vertex_buffer(0, buffer.get());
    list.end_pass();
    rhi.submit(list);
    CHECK(raw->ref_count() == 2);
    CHECK(list.empty());

    const size_t live_before = rhi.live_resources();
    buffer.reset();
    CHECK(rhi.live_resources() == live_before);

    rhi.present(viewport.get());
    CHECK(rhi.live_resources() == live_before);
    rhi.end_frame();
    CHECK(rhi.live_resources() == live_before - 1);
}

TEST_CASE("NullRHI buffers store updates and clears write the colour")
{
    NullRHI rhi;
    RHIBufferPtr buffer = rhi.create_buffer({ .size = 4 });
    const std::array<uint8_t, 2> data = { uint8_t{ 7 }, uint8_t{ 9 } };
    buffer->update(1, data.data(), static_cast<uint32_t>(data.size()));
    const std::vector<uint8_t>& bytes = static_cast<NullBuffer&>(*buffer).bytes();
    CHECK(bytes[0] == uint8_t{ 0 });
    CHECK(bytes[1] == uint8_t{ 7 });
    CHECK(bytes[2] == uint8_t{ 9 });

    RHITexturePtr texture = rhi.create_texture({ .width = 1, .height = 1, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    RHICommandList list;
    list.begin_pass(target.get(), { Colour{ 1.0f, 0.0f, 0.0f, 1.0f }, true });
    list.end_pass();
    rhi.submit(list);

    std::array<uint8_t, 4> pixel{};
    rhi.read_texture(texture.get(), pixel.data(), static_cast<uint32_t>(pixel.size()));
    CHECK(pixel[0] == uint8_t{ 0 });
    CHECK(pixel[2] == uint8_t{ 255 });
    CHECK(pixel[3] == uint8_t{ 255 });
}

TEST_CASE("NullRHI rejects mismatched shader stages and unsupported render target formats")
{
    NullRHI rhi;
    CHECK_THROWS_AS(rhi.create_vertex_shader({ .stage = RHIShaderStage::Pixel }), Error);
    CHECK_THROWS_AS(rhi.create_pixel_shader({ .stage = RHIShaderStage::Compute }), Error);
    RHITexturePtr depth = rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::RenderTarget });
    CHECK_THROWS_AS(rhi.create_render_target({ .colour = depth }), Error);
}

TEST_CASE("NullRHI collects released resources on the next end_frame")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 4 });
        RHITexturePtr texture = rhi.create_texture({ .width = 1, .height = 1, .usage = RHITextureUsage::RenderTarget });
        RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
        CHECK(rhi.live_resources() == 4);
    }
    CHECK(rhi.live_resources() == 4);
    rhi.end_frame();
    CHECK(rhi.live_resources() == 1);
}

TEST_CASE("NullRHI wait_idle completes in-flight frames without present")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHITexturePtr texture = rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    const size_t baseline = rhi.live_resources();
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 16 });
        RHICommandList list;
        list.begin_pass(target.get());
        list.set_vertex_buffer(0, buffer.get());
        list.end_pass();
        rhi.submit(list);
    }
    CHECK(rhi.live_resources() == baseline + 1);
    rhi.wait_idle();
    CHECK(rhi.live_resources() == baseline);
}

TEST_CASE("NullRHI frame slots do not grow over many frames")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    RHICommandList list;
    for (int32_t frame = 0; frame < 1000; ++frame)
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 8 });
        list.begin_pass(back_buffer.get());
        list.set_vertex_buffer(0, buffer.get());
        list.end_pass();
        rhi.submit(list);
        rhi.present(viewport.get());
        rhi.end_frame();
        CHECK(RHIResource::retired_pending() <= 1);
    }
    rhi.wait_idle();
    CHECK(rhi.live_resources() == 3);
}

TEST_CASE("NullRHI present without a back buffer does not advance the frame, end_frame does")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport(viewport_desc());
    const uint64_t frame = RHIResource::frame_serial();
    CHECK_NOTHROW(rhi.present(viewport.get()));
    CHECK(RHIResource::frame_serial() == frame);
    CHECK(rhi.frame_count() == 0);
    rhi.end_frame();
    CHECK(RHIResource::frame_serial() == frame + 1);
    CHECK(rhi.frame_count() == 1);
    CHECK(RHIResource::retired_pending() == 0);
}

TEST_CASE("NullRHI end_frame without present still retires submitted resources")
{
    NullRHI rhi;
    RHITexturePtr texture = rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::RenderTarget });
    RHIRenderTargetPtr target = rhi.create_render_target({ .colour = texture });
    const size_t baseline = rhi.live_resources();
    {
        RHIBufferPtr buffer = rhi.create_buffer({ .size = 16 });
        RHICommandList list;
        list.begin_pass(target.get());
        list.set_vertex_buffer(0, buffer.get());
        list.end_pass();
        rhi.submit(list);
    }
    CHECK(rhi.live_resources() == baseline + 1);
    rhi.end_frame();
    CHECK(rhi.live_resources() == baseline);
}

TEST_CASE("NullRHI present copies a source texture into the back buffer")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 2, .height = 1, .format = RHIFormat::RGBA8Unorm });
    const std::array<uint8_t, 8> pixels = { 1, 2, 3, 4, 5, 6, 7, 8 };
    RHITexturePtr source = rhi.create_texture({ .width = 2, .height = 1, .format = RHIFormat::RGBA8Unorm, .initial_data = pixels.data(), .initial_data_size = static_cast<uint32_t>(pixels.size()) });

    rhi.present(viewport.get(), source.get());

    RHIRenderTargetPtr back_buffer = viewport->acquire_back_buffer();
    const NullRenderTarget& target = static_cast<NullRenderTarget&>(*back_buffer);
    CHECK(target.texture().bytes() == std::vector<uint8_t>(pixels.begin(), pixels.end()));
}

TEST_CASE("NullRHI present rejects a mismatched or unsampled source")
{
    NullRHI rhi;
    RHIViewportPtr viewport = rhi.create_viewport({ .width = 2, .height = 2, .format = RHIFormat::RGBA8Unorm });
    RHITexturePtr wrong_size = rhi.create_texture({ .width = 3, .height = 2, .format = RHIFormat::RGBA8Unorm });
    RHITexturePtr wrong_format = rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::BGRA8Unorm });
    RHITexturePtr unsampled = rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::RGBA8Unorm, .usage = RHITextureUsage::RenderTarget });
    CHECK_THROWS_AS(rhi.present(viewport.get(), wrong_size.get()), Error);
    CHECK_THROWS_AS(rhi.present(viewport.get(), wrong_format.get()), Error);
    CHECK_THROWS_AS(rhi.present(viewport.get(), unsampled.get()), Error);
}

TEST_CASE("Two NullRHI devices can be alive at once")
{
    NullRHI first;
    NullRHI second;
    RHIBufferPtr buffer = first.create_buffer({ .size = 4 });
    RHIViewportPtr viewport = second.create_viewport(viewport_desc());
    buffer.reset();
    second.end_frame();
    CHECK(RHIResource::retired_pending() == 0);
}

namespace
{

RHIGraphicsPipelineDesc minimal_pipeline_desc(NullRHI& rhi)
{
    RHIGraphicsPipelineDesc desc;
    desc.vertex = rhi.create_vertex_shader({ .stage = RHIShaderStage::Vertex });
    desc.pixel = rhi.create_pixel_shader({ .stage = RHIShaderStage::Pixel });
    desc.colour_formats[0] = RHIFormat::RGBA8Unorm;
    return desc;
}

} // namespace

TEST_CASE("NullRHI pipelines accept every blend preset, MRT and depth")
{
    NullRHI rhi;
    for (const RHIBlendState& blend : { rhi_blend_opaque(), rhi_blend_alpha(), rhi_blend_additive() })
    {
        RHIGraphicsPipelineDesc desc = minimal_pipeline_desc(rhi);
        desc.blend[0] = blend;
        CHECK_NOTHROW(rhi.create_graphics_pipeline(desc));
    }
    CHECK_FALSE(rhi_blend_opaque().enabled);
    CHECK(rhi_blend_alpha().src_colour == RHIBlendFactor::SrcAlpha);
    CHECK(rhi_blend_additive().dst_colour == RHIBlendFactor::One);

    RHIGraphicsPipelineDesc mrt = minimal_pipeline_desc(rhi);
    mrt.colour_formats[1] = RHIFormat::BGRA8Unorm;
    mrt.colour_format_count = 2;
    mrt.depth_format = RHIFormat::Depth32Float;
    mrt.depth_stencil.depth_test = true;
    mrt.depth_stencil.depth_write = true;
    RHIGraphicsPipelinePtr pipeline = rhi.create_graphics_pipeline(mrt);
    CHECK(pipeline->colour_format_count() == 2);
    CHECK(pipeline->colour_formats()[1] == RHIFormat::BGRA8Unorm);
    CHECK(pipeline->depth_format() == RHIFormat::Depth32Float);
    CHECK(pipeline->kind() == RHIPipelineKind::Graphics);
}

TEST_CASE("NullRHI pipelines expose their binding table")
{
    NullRHI rhi;
    const RHIBindingDesc bindings[] = {
        { .kind = RHIBindingKind::Constants, .stage_mask = RHIShaderStageMask::Vertex, .slot = 0, .size = 64 },
        { .kind = RHIBindingKind::SampledTexture, .stage_mask = RHIShaderStageMask::Pixel, .slot = 0, .array_count = 16 },
    };
    RHIGraphicsPipelineDesc desc = minimal_pipeline_desc(rhi);
    desc.bindings = bindings;
    desc.binding_count = 2;
    RHIGraphicsPipelinePtr pipeline = rhi.create_graphics_pipeline(desc);
    REQUIRE(pipeline->binding_count() == 2);
    CHECK(pipeline->binding(0).size == 64);
    CHECK(pipeline->binding(1).array_count == 16);
    CHECK(pipeline->bindings()[1].kind == RHIBindingKind::SampledTexture);
    CHECK_THROWS_AS(pipeline->binding(2), Error);
    CHECK_THROWS_AS(pipeline->binding(RHI_INVALID_BINDING), Error);
}

TEST_CASE("NullRHI rejects malformed pipeline descriptions")
{
    NullRHI rhi;
    const RHIVertexAttribute attributes[] = { { .location = 0, .format = RHIVertexFormat::Float4, .offset = 0, .slot = 0 } };

    RHIGraphicsPipelineDesc no_shader = minimal_pipeline_desc(rhi);
    no_shader.pixel = {};
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(no_shader), Error);

    RHIGraphicsPipelineDesc no_targets = minimal_pipeline_desc(rhi);
    no_targets.colour_format_count = 0;
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(no_targets), Error);

    RHIGraphicsPipelineDesc too_many_targets = minimal_pipeline_desc(rhi);
    too_many_targets.colour_format_count = RHI_MAX_COLOUR_TARGETS + 1;
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(too_many_targets), Error);

    RHIGraphicsPipelineDesc depth_as_colour = minimal_pipeline_desc(rhi);
    depth_as_colour.colour_formats[0] = RHIFormat::Depth32Float;
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(depth_as_colour), Error);

    RHIGraphicsPipelineDesc colour_as_depth = minimal_pipeline_desc(rhi);
    colour_as_depth.depth_format = RHIFormat::RGBA8Unorm;
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(colour_as_depth), Error);

    RHIGraphicsPipelineDesc bad_samples = minimal_pipeline_desc(rhi);
    bad_samples.sample_count = 3;
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(bad_samples), Error);

    RHIGraphicsPipelineDesc bad_slot = minimal_pipeline_desc(rhi);
    bad_slot.vertex_input.attributes = attributes;
    bad_slot.vertex_input.attribute_count = 1;
    bad_slot.vertex_input.streams[0].stride = 16;
    CHECK_NOTHROW(rhi.create_graphics_pipeline(bad_slot));
    bad_slot.vertex_input.streams[0].stride = 8;
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(bad_slot), Error);

    const RHIVertexAttribute out_of_range[] = { { .location = 0, .slot = RHI_MAX_VERTEX_SLOTS } };
    RHIGraphicsPipelineDesc bad_stream = minimal_pipeline_desc(rhi);
    bad_stream.vertex_input.attributes = out_of_range;
    bad_stream.vertex_input.attribute_count = 1;
    CHECK_THROWS_AS(rhi.create_graphics_pipeline(bad_stream), Error);
}

TEST_CASE("NullRHI rejects malformed binding tables")
{
    NullRHI rhi;
    const RHIShaderStageMask pixel = RHIShaderStageMask::Pixel;

    auto create = [&](const std::vector<RHIBindingDesc>& bindings) {
        RHIGraphicsPipelineDesc desc = minimal_pipeline_desc(rhi);
        desc.bindings = bindings.data();
        desc.binding_count = static_cast<uint32_t>(bindings.size());
        return rhi.create_graphics_pipeline(desc);
    };

    CHECK_THROWS_AS(create(std::vector<RHIBindingDesc>(RHI_MAX_BINDINGS + 1, RHIBindingDesc{ .kind = RHIBindingKind::Sampler, .stage_mask = pixel })), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::Constants, .stage_mask = pixel, .size = 0 } }), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::Constants, .stage_mask = pixel, .size = RHI_MAX_CONSTANTS_SIZE + 1 } }), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::Sampler, .stage_mask = pixel, .array_count = 0 } }), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::UniformBuffer, .stage_mask = pixel, .array_count = 2 } }), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::Sampler, .stage_mask = static_cast<RHIShaderStageMask>(0) } }), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::StorageBuffer, .stage_mask = pixel } }), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::StorageTexture, .stage_mask = pixel } }), Error);

    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::Sampler, .stage_mask = pixel, .slot = 0 },
                             { .kind = RHIBindingKind::Sampler, .stage_mask = pixel, .slot = 0 } }), Error);
    CHECK_THROWS_AS(create({ { .kind = RHIBindingKind::SampledTexture, .stage_mask = pixel, .slot = 0, .array_count = 4 },
                             { .kind = RHIBindingKind::SampledTexture, .stage_mask = pixel, .slot = 3 } }), Error);
    CHECK_NOTHROW(create({ { .kind = RHIBindingKind::Sampler, .stage_mask = pixel, .slot = 0 },
                           { .kind = RHIBindingKind::SampledTexture, .stage_mask = pixel, .slot = 0 },
                           { .kind = RHIBindingKind::Constants, .stage_mask = RHIShaderStageMask::Vertex, .slot = 0, .size = 16 },
                           { .kind = RHIBindingKind::Constants, .stage_mask = pixel, .slot = 0, .size = 16 } }));
}

TEST_CASE("NullRHI textures support arrays, cubes and multisample but not mips or 3D")
{
    NullRHI rhi;
    RHITexturePtr array = rhi.create_texture({ .width = 2, .height = 2, .dimension = RHITextureDimension::Tex2DArray, .array_layers = 3 });
    CHECK(array->dimension() == RHITextureDimension::Tex2DArray);
    CHECK(array->array_layers() == 3);
    std::vector<uint8_t> bytes(2 * 2 * 4 * 3);
    CHECK_NOTHROW(rhi.read_texture(array.get(), bytes.data(), static_cast<uint32_t>(bytes.size())));

    CHECK_NOTHROW(rhi.create_texture({ .width = 2, .height = 2, .dimension = RHITextureDimension::Cube, .array_layers = 6 }));
    CHECK_THROWS_AS(rhi.create_texture({ .width = 2, .height = 3, .dimension = RHITextureDimension::Cube, .array_layers = 6 }), Error);
    CHECK_THROWS_AS(rhi.create_texture({ .width = 2, .height = 2, .array_layers = 2 }), Error);
    CHECK_THROWS_AS(rhi.create_texture({ .width = 2, .height = 2, .dimension = RHITextureDimension::Tex3D }), Error);
    CHECK_THROWS_AS(rhi.create_texture({ .width = 2, .height = 2, .mip_levels = 2 }), Error);

    RHITexturePtr multisample = rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::RenderTarget, .dimension = RHITextureDimension::Tex2DMultisample, .sample_count = 4 });
    CHECK(multisample->sample_count() == 4);
    CHECK_THROWS_AS(rhi.create_render_target({ .colour = multisample }), Error);
    CHECK_THROWS_AS(rhi.create_texture({ .width = 2, .height = 2, .dimension = RHITextureDimension::Tex2DMultisample, .sample_count = 3 }), Error);
    CHECK_THROWS_AS(rhi.create_texture({ .width = 2, .height = 2, .dimension = RHITextureDimension::Tex2DMultisample }), Error);

    CHECK_THROWS_AS(rhi.create_texture({ .width = 2, .height = 2, .usage = RHITextureUsage::DepthStencil }), Error);
    CHECK_NOTHROW(rhi.create_texture({ .width = 2, .height = 2, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::DepthStencil }));
}

TEST_CASE("NullRHI executes MRT and depth passes, state commands and debug groups")
{
    NullRHI rhi;
    RHITexturePtr first_texture = rhi.create_texture({ .width = 2, .height = 1, .usage = RHITextureUsage::RenderTarget });
    RHITexturePtr second_texture = rhi.create_texture({ .width = 2, .height = 1, .format = RHIFormat::BGRA8Unorm, .usage = RHITextureUsage::RenderTarget });
    RHITexturePtr depth = rhi.create_texture({ .width = 2, .height = 1, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::DepthStencil });
    RHIRenderTargetPtr first = rhi.create_render_target({ .colour = first_texture });
    RHIRenderTargetPtr second = rhi.create_render_target({ .colour = second_texture });

    const RHIBindingDesc binding = { .kind = RHIBindingKind::Constants, .stage_mask = RHIShaderStageMask::Vertex, .size = 16 };
    RHIGraphicsPipelineDesc desc = minimal_pipeline_desc(rhi);
    desc.colour_formats[1] = RHIFormat::BGRA8Unorm;
    desc.colour_format_count = 2;
    desc.depth_format = RHIFormat::Depth32Float;
    desc.bindings = &binding;
    desc.binding_count = 1;
    RHIGraphicsPipelinePtr pipeline = rhi.create_graphics_pipeline(desc);

    RHIRenderPassDesc pass;
    pass.colour[0].target = first.get();
    pass.colour[0].clear_colour = Colour{ 1.0f, 0.0f, 0.0f, 1.0f };
    pass.colour[1].target = second.get();
    pass.colour[1].clear_colour = Colour{ 1.0f, 0.0f, 0.0f, 1.0f };
    pass.colour_count = 2;
    pass.depth.texture = depth.get();
    pass.depth.clear_depth = 0.5f;

    const std::array<float, 4> constants = { 1.0f, 2.0f, 3.0f, 4.0f };
    RHICommandList list;
    list.begin_pass(pass);
    list.push_debug_group("scene");
    list.set_pipeline(pipeline.get());
    list.set_viewport({ 0.0f, 0.0f, 2.0f, 1.0f });
    list.set_scissor({ 0, 0, 1, 1 });
    list.set_constants(0, constants.data(), sizeof(constants));
    list.draw(3);
    list.pop_debug_group();
    list.end_pass();
    rhi.submit(list);

    std::array<uint8_t, 8> pixels{};
    rhi.read_texture(first_texture.get(), pixels.data(), static_cast<uint32_t>(pixels.size()));
    CHECK(pixels[0] == 255);
    CHECK(pixels[2] == 0);
    rhi.read_texture(second_texture.get(), pixels.data(), static_cast<uint32_t>(pixels.size()));
    CHECK(pixels[0] == 0);
    CHECK(pixels[2] == 255);

    std::array<float, 2> depths{};
    rhi.read_texture(depth.get(), reinterpret_cast<uint8_t*>(depths.data()), sizeof(depths));
    CHECK(depths[0] == 0.5f);
    CHECK(depths[1] == 0.5f);

    const NullStats& stats = rhi.stats();
    CHECK(stats.passes == 1);
    CHECK(stats.draw_calls == 1);
    CHECK(stats.last_viewport.width == 2.0f);
    CHECK(stats.last_scissor.width == 1);
    CHECK(stats.last_constants_binding == 0);
    CHECK(stats.last_constants.size() == sizeof(constants));
    CHECK(stats.debug_events == std::vector<std::string>{ "push:scene", "pop" });
}

TEST_CASE("NullRHI submit rejects an unbalanced debug group")
{
    NullRHI rhi;
    RHICommandList list;
    list.push_debug_group("open");
    CHECK_THROWS_AS(rhi.submit(list), Error);
}
