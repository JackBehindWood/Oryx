#include "oxpch.h"
#include "Oryx/Renderer/Scene/SceneRenderer.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

// Opens a batcher scene for its lifetime; a scene closed behind its back throws on exit, except while another exception is unwinding.
class BatcherScope
{
public:
    BatcherScope(BatchRenderer2D& batcher, const Camera& camera, const BatchTarget& target)
        : m_batcher(batcher)
    {
        m_batcher.begin(camera, target);
    }

    ~BatcherScope() noexcept(false)
    {
        if (std::uncaught_exceptions() > 0)
        {
            try
            {
                m_batcher.end();
            }
            catch (const Error& error)
            {
                error.log();
            }
            return;
        }
        m_batcher.end();
    }

    BatcherScope(const BatcherScope&) = delete;
    BatcherScope& operator=(const BatcherScope&) = delete;

private:
    BatchRenderer2D& m_batcher;
};

std::vector<UniquePtr<RenderPass>> make_main_pass(const BatchRendererDesc& batch)
{
    std::vector<UniquePtr<RenderPass>> passes;
    UniquePtr<RenderPass> main = create_unique<RenderPass>();
    main->id = RENDER_PASS_MAIN;
    main->desc.name = "Main";
    main->formats = { batch.colour_format, RHIFormat::Undefined };
    passes.push_back(std::move(main));
    return passes;
}

BatchRendererDesc with_sink(const BatchRendererDesc& batch, std::vector<DrawItem>& sink)
{
    return { batch.rhi, batch.pipelines, batch.memo, batch.shaders, batch.defaults, sink, batch.colour_format, batch.page_bytes, batch.max_indexed_primitives, batch.max_texture_bindings };
}

bool batched_2d(RenderStage stage)
{
    return stage == RenderStage::Scene2D || stage == RenderStage::Overlay;
}

} // namespace

SceneRenderer::SceneRenderer(const SceneRendererDesc& desc)
    : m_passes(make_main_pass(desc.batch))
    , m_batcher(with_sink(desc.batch, m_passes[RENDER_PASS_MAIN]->items))
    , m_debug(desc.debug)
{
    for (RenderPassId& route : m_route)
    {
        route = RENDER_PASS_NONE;
    }
    for (RenderStage stage : { RenderStage::Opaque3D, RenderStage::Transparent3D, RenderStage::Scene2D, RenderStage::Overlay })
    {
        m_route[static_cast<uint32_t>(stage)] = RENDER_PASS_MAIN;
    }
    rebuild_order();
}

void SceneRenderer::begin_scene(const RenderView& view)
{
    if (m_open)
    {
        throw Error("A scene is already open", "call end_scene first");
    }
    m_camera = &view.camera;
    m_logical = view.logical;
    m_framebuffer = view.framebuffer;
    m_scale = view.scale;
    m_sources.clear();
    m_open = true;
}

void SceneRenderer::submit(RenderSource& source)
{
    if (!m_open)
    {
        throw Error("submit needs an open scene", "call begin_scene first");
    }
    m_sources.push_back(&source);
}

void SceneRenderer::end_scene()
{
    if (!m_open)
    {
        throw Error("end_scene without an open scene", "call begin_scene first");
    }
    struct Close
    {
        SceneRenderer& scene;
        ~Close()
        {
            scene.m_sources.clear();
            scene.m_open = false;
        }
    } close{ *this };

    if (m_sources.empty() && (m_debug == nullptr || m_debug->size() == 0))
    {
        return;
    }
    const RenderView view{ *m_camera, m_logical, m_framebuffer, m_scale };
    for (uint32_t index = 0; index < RENDER_STAGE_COUNT; ++index)
    {
        run_stage(static_cast<RenderStage>(index), view);
    }
}

void SceneRenderer::render(const RenderView& view, RenderSource& source)
{
    SceneScope scope(*this, view);
    submit(source);
}

void SceneRenderer::run_stage(RenderStage stage, const RenderView& view)
{
    const RenderPassId id = stage_pass(stage);
    if (id == RENDER_PASS_NONE)
    {
        return;
    }
    RenderPass& target = *m_passes[id];
    StageContext context{ view, target, m_batcher };
    if (!batched_2d(stage))
    {
        for (RenderSource* source : m_sources)
        {
            source->render_stage(stage, context);
        }
        return;
    }

    auto run_batched = [&](const Camera& camera)
    {
        BatcherScope scope(m_batcher, camera, BatchTarget{ target.items, target.formats });
        for (RenderSource* source : m_sources)
        {
            source->render_stage(stage, context);
        }
        if (stage == RenderStage::Scene2D && m_debug != nullptr)
        {
            m_debug->render(m_batcher);
        }
    };
    if (stage == RenderStage::Scene2D)
    {
        run_batched(view.camera);
    }
    else if (view.logical[0] > 0.0f && view.logical[1] > 0.0f)
    {
        run_batched(Camera2D::screen_space(view.logical[0], view.logical[1]));
    }
}

RenderPassId SceneRenderer::add_pass(const RenderPassDesc& desc)
{
    if (desc.depth && !rhi_format_is_depth(desc.depth->format()))
    {
        throw Error("A pass depth attachment needs a depth format", "pass '" + desc.name + "'");
    }
    UniquePtr<RenderPass> pass = create_unique<RenderPass>();
    pass->id = static_cast<RenderPassId>(m_passes.size());
    pass->desc = desc;
    pass->formats.colour = desc.colour ? desc.colour->format() : m_passes[RENDER_PASS_MAIN]->formats.colour;
    pass->formats.depth = desc.depth ? desc.depth->format() : RHIFormat::Undefined;
    m_passes.push_back(std::move(pass));
    return m_passes.back()->id;
}

void SceneRenderer::remove_pass(RenderPassId id)
{
    if (id == RENDER_PASS_MAIN)
    {
        throw Error("The Main pass cannot be removed");
    }
    static_cast<void>(pass(id));
    for (RenderPassId& route : m_route)
    {
        if (route == id)
        {
            route = RENDER_PASS_NONE;
        }
    }
    m_passes[id].reset();
    rebuild_order();
}

void SceneRenderer::route_stage(RenderStage stage, RenderPassId id)
{
    if (id != RENDER_PASS_NONE)
    {
        static_cast<void>(pass(id));
    }
    RenderPassId routes[RENDER_STAGE_COUNT];
    std::copy(std::begin(m_route), std::end(m_route), std::begin(routes));
    routes[static_cast<uint32_t>(stage)] = id;

    std::vector<RenderPassId> seen;
    RenderPassId previous = RENDER_PASS_NONE;
    for (RenderPassId route : routes)
    {
        if (route == RENDER_PASS_NONE || route == previous)
        {
            continue;
        }
        if (std::find(seen.begin(), seen.end(), route) != seen.end())
        {
            throw Error("A pass must cover consecutive stages", "pass '" + m_passes[route]->desc.name + "' would skip a stage routed elsewhere");
        }
        seen.push_back(route);
        previous = route;
    }
    std::copy(std::begin(routes), std::end(routes), std::begin(m_route));
    rebuild_order();
}

RenderPass& SceneRenderer::pass(RenderPassId id)
{
    if (id >= m_passes.size() || !m_passes[id])
    {
        throw Error("Unknown render pass", "id " + std::to_string(id));
    }
    return *m_passes[id];
}

void SceneRenderer::rebuild_order()
{
    m_order.clear();
    for (RenderPassId route : m_route)
    {
        if (route == RENDER_PASS_NONE)
        {
            continue;
        }
        RenderPass* routed = m_passes[route].get();
        if (m_order.empty() || m_order.back() != routed)
        {
            m_order.push_back(routed);
        }
    }
}

void SceneRenderer::clear_items()
{
    for (const UniquePtr<RenderPass>& pass : m_passes)
    {
        if (pass)
        {
            pass->items.clear();
        }
    }
}

void SceneRenderer::finish_frame(uint32_t frame_slot)
{
    if (m_open)
    {
        throw Error("A scene is still open at the end of the frame", "call end_scene before the frame is recorded");
    }
    if (m_batcher.open())
    {
        throw Error("A batcher scene is still open at the end of the frame", "call end_scene before the frame is recorded");
    }
    m_batcher.recycle(frame_slot);
}

SceneScope::SceneScope(SceneRenderer& scene, const RenderView& view)
    : m_scene(scene)
{
    m_scene.begin_scene(view);
}

SceneScope::~SceneScope() noexcept(false)
{
    if (std::uncaught_exceptions() > 0)
    {
        try
        {
            m_scene.end_scene();
        }
        catch (const Error& error)
        {
            error.log();
        }
        return;
    }
    m_scene.end_scene();
}

} // namespace oryx
