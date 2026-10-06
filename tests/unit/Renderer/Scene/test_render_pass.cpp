#include "doctest.h"

#include "Oryx.h"
#include "NullRHI.h"
#include "unit/Renderer/RenderTestSupport.h"

using namespace oryx;
using namespace oryx::test;

namespace
{

struct RendererGuard
{
    RendererGuard() { Renderer::init({ RHIBackend::Null }); }
    ~RendererGuard() { Renderer::shutdown(); }
};

struct PassFixture
{
    NullRHI& rhi;
    RHIViewportPtr viewport;

    PassFixture()
        : rhi(static_cast<NullRHI&>(Renderer::rhi()))
    {
        viewport = rhi.create_viewport({ .width = 8, .height = 8 });
        Renderer::set_viewport(viewport);
    }

    ~PassFixture() { Renderer::set_viewport({}); }

    RenderPassDesc shadow_desc()
    {
        RenderTarget target = RenderTarget::create(rhi, 16, 16);
        RenderPassDesc desc;
        desc.name = "Shadow";
        desc.colour = target.rhi_ptr();
        desc.depth = rhi.create_texture({ .width = 16, .height = 16, .format = RHIFormat::Depth32Float, .usage = RHITextureUsage::DepthStencil });
        return desc;
    }

    DrawItem triangle(const PassFormats& formats)
    {
        DrawItem item;
        item.pipeline = Renderer::pipeline(pipeline_def(Primitive2D::Triangle), formats);
        item.vertex_buffers[0] = rhi.create_buffer({ .size = 256, .usage = RHIBufferUsage::Vertex });
        item.vertex_count = 3;
        return item;
    }

    std::vector<std::string_view> commands() const
    {
        std::vector<std::string_view> out;
        for (std::string_view command : rhi.last_submission())
        {
            if (command != "PushDebugGroup" && command != "PopDebugGroup")
            {
                out.push_back(command);
            }
        }
        return out;
    }
};

} // namespace

TEST_CASE("RenderPass: Main is the only routed pass by default")
{
    RendererGuard guard;
    SceneRenderer& scene = Renderer::scene();
    REQUIRE(scene.ordered_passes().size() == 1);
    CHECK(scene.ordered_passes()[0]->id == RENDER_PASS_MAIN);
    CHECK(scene.stage_pass(RenderStage::Shadow) == RENDER_PASS_NONE);
    CHECK(scene.stage_pass(RenderStage::Opaque3D) == RENDER_PASS_MAIN);
    CHECK(scene.stage_pass(RenderStage::Overlay) == RENDER_PASS_MAIN);
    CHECK(scene.stage_pass(RenderStage::PostProcess) == RENDER_PASS_NONE);
    CHECK_THROWS_AS(scene.remove_pass(RENDER_PASS_MAIN), Error);
    CHECK_THROWS_AS(static_cast<void>(scene.pass(7)), Error);
}

TEST_CASE("RenderPass: an offscreen depth pass routed to Shadow records before Main")
{
    RendererGuard guard;
    PassFixture fixture;
    SceneRenderer& scene = Renderer::scene();
    const RenderPassId shadow = scene.add_pass(fixture.shadow_desc());
    CHECK(scene.pass(shadow).formats == PassFormats{ RHIFormat::RGBA8Unorm, RHIFormat::Depth32Float });
    scene.route_stage(RenderStage::Shadow, shadow);
    REQUIRE(scene.ordered_passes().size() == 2);
    CHECK(scene.ordered_passes()[0]->id == shadow);

    const PassFormats shadow_formats = scene.pass(shadow).formats;
    Camera camera(perspective(1.0f, 1.0f, 0.1f, 10.0f), look_at(Vec3f{ 0.0f, 0.0f, -3.0f }, Vec3f{ 0.0f, 0.0f, 0.0f }, Vec3f{ 0.0f, 1.0f, 0.0f }));
    const Camera* seen = nullptr;
    FnSource source([&](RenderStage stage, StageContext& context)
    {
        if (stage == RenderStage::Shadow)
        {
            CHECK(&context.pass == &scene.pass(shadow));
            CHECK_FALSE(context.batcher_2d.open());
            context.emit(fixture.triangle(shadow_formats));
        }
        else if (stage == RenderStage::Opaque3D)
        {
            seen = &context.view.camera;
            CHECK(context.pass.id == RENDER_PASS_MAIN);
        }
    });
    scene.render(view_over(camera), source);
    CHECK(seen == &camera);
    CHECK(scene.pass(shadow).items.size() == 1);

    CHECK(Renderer::end_frame());
    const std::vector<std::string_view> commands = fixture.commands();
    CHECK(std::count(commands.begin(), commands.end(), "BeginPass") == 2);
    const auto first_end = std::find(commands.begin(), commands.end(), "EndPass");
    const auto draw = std::find(commands.begin(), commands.end(), "Draw");
    REQUIRE(draw != commands.end());
    CHECK(draw < first_end);
    CHECK(scene.pass(shadow).items.empty());
}

TEST_CASE("RenderPass: routing keeps a pass on consecutive stages")
{
    RendererGuard guard;
    PassFixture fixture;
    SceneRenderer& scene = Renderer::scene();
    const RenderPassId shadow = scene.add_pass(fixture.shadow_desc());
    scene.route_stage(RenderStage::Shadow, shadow);
    CHECK_THROWS_AS(scene.route_stage(RenderStage::PostProcess, shadow), Error);
    CHECK(scene.stage_pass(RenderStage::PostProcess) == RENDER_PASS_NONE);
    CHECK_NOTHROW(scene.route_stage(RenderStage::Opaque3D, shadow));
    CHECK_NOTHROW(scene.route_stage(RenderStage::Opaque3D, RENDER_PASS_MAIN));
    CHECK_THROWS_AS(scene.route_stage(RenderStage::Shadow, 99), Error);
}

TEST_CASE("RenderPass: empty offscreen passes are skipped, Main still clears, and a removed pass un-routes")
{
    RendererGuard guard;
    PassFixture fixture;
    SceneRenderer& scene = Renderer::scene();
    const RenderPassId shadow = scene.add_pass(fixture.shadow_desc());
    scene.route_stage(RenderStage::Shadow, shadow);

    CHECK(Renderer::end_frame());
    CHECK(fixture.commands() == std::vector<std::string_view>{ "BeginPass", "EndPass" });

    scene.remove_pass(shadow);
    CHECK(scene.stage_pass(RenderStage::Shadow) == RENDER_PASS_NONE);
    CHECK(scene.ordered_passes().size() == 1);
    CHECK_THROWS_AS(scene.remove_pass(shadow), Error);

    for (RenderStage stage : { RenderStage::Opaque3D, RenderStage::Transparent3D, RenderStage::Scene2D, RenderStage::Overlay })
    {
        scene.route_stage(stage, RENDER_PASS_NONE);
    }
    CHECK(Renderer::end_frame());
    CHECK(fixture.commands() == std::vector<std::string_view>{ "BeginPass", "EndPass" });
}

TEST_CASE("RenderPass: a depth attachment must have a depth format")
{
    RendererGuard guard;
    PassFixture fixture;
    RenderPassDesc desc = fixture.shadow_desc();
    desc.depth = fixture.rhi.create_texture({ .width = 16, .height = 16, .format = RHIFormat::RGBA8Unorm, .usage = RHITextureUsage::Sampled });
    CHECK_THROWS_AS(static_cast<void>(Renderer::scene().add_pass(desc)), Error);
}

TEST_CASE("RenderPass: pipelines for different pass formats coexist")
{
    RendererGuard guard;
    const PipelineDef& def = pipeline_def(Primitive2D::Triangle);
    const GraphicsPipelineHandle back = Renderer::pipeline(def);
    const GraphicsPipelineHandle offscreen = Renderer::pipeline(def, PassFormats{ RHIFormat::RGBA8Unorm, RHIFormat::Depth32Float });
    CHECK(back.index != offscreen.index);
    CHECK(Renderer::pipeline(def) .index == back.index);
    CHECK(Renderer::pipeline(def, PassFormats{ RHIFormat::RGBA8Unorm, RHIFormat::Depth32Float }).index == offscreen.index);
    CHECK(Renderer::resolve_pipeline(offscreen).rhi().depth_format() == RHIFormat::Depth32Float);
}
