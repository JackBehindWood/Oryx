#pragma once

#include "Oryx/Core/ViewRegion.h"
#include "Oryx/Renderer/Batch/BatchRenderer2D.h"
#include "Oryx/Renderer/Batch/DebugRenderer.h"
#include "Oryx/Renderer/Scene/RenderPass.h"
#include "Oryx/Renderer/Scene/RenderStage.h"
#include "Oryx/Renderer/Scene/RenderView.h"

namespace oryx
{

// What a stage callback gets: the view, the pass its draws land in, and the 2D batcher (open under the stage's camera for Scene2D and Overlay, closed in the others).
struct StageContext
{
    const RenderView& view;
    RenderPass& pass;
    BatchRenderer2D& batcher_2d;

    // Appends a DrawItem to the pass, for emitters that do not batch (meshes and the like).
    void emit(DrawItem item) { pass.items.push_back(std::move(item)); }
};

// Something a frame draws. One virtual, called once per routed stage in stage order, so a new stage never breaks an existing source: ignore the stages you do not draw in.
class RenderSource
{
public:
    virtual ~RenderSource() = default;

    virtual void render_stage(RenderStage, StageContext&) {}
};

// Sources draw in layer order, then submission order; the interface layer draws on top of the world.
constexpr int32_t k_layer_world = 0;
constexpr int32_t k_layer_interface = 100;

// Where the sources submitted next draw: a sub-view of the scene's surface (logical points, origin top left) and their draw layer.
struct SubmitContext
{
    ViewRegion region;
    bool has_region = false;
    int32_t layer = k_layer_world;
};

struct SceneRendererDesc
{
    BatchRendererDesc batch;
    // Replayed at the end of Scene2D; null replays nothing.
    DebugRenderer* debug = nullptr;
};

// Owns a frame's 2D drawing and its ordered pass list. A scene is opened for one view, sources are submitted, and end_scene runs every stage across all sources
// (stage-major, in submission order within a stage) into the pass each stage is routed to; the frame owner then records the passes in order.
// A sibling of the batchers, not a BatchRenderer; it opens no GPU pass and ends no frame, which stay with record_frame.
class SceneRenderer
{
public:
    explicit SceneRenderer(const SceneRendererDesc& desc);

    SceneRenderer(const SceneRenderer&) = delete;
    SceneRenderer& operator=(const SceneRenderer&) = delete;

    // Throws Error when a scene is already open. The view's camera must outlive end_scene.
    void begin_scene(const RenderView& view);
    // Queues a source for the open scene under the current submit context; it must outlive end_scene. Throws Error outside a scene.
    void submit(RenderSource& source);
    // A source with a region sees a StageContext view of the region's size and draws clipped to it; screen-space scenes only.
    void set_submit_context(const SubmitContext& context) { m_context = context; }
    [[nodiscard]] const SubmitContext& submit_context() const { return m_context; }
    // Runs the stages and closes the scene even when a stage throws. Throws Error outside a scene.
    void end_scene();
    void render(const RenderView& view, RenderSource& source);
    [[nodiscard]] bool open() const { return m_open; }

    // Pass 0 is Main, the back buffer. A pass added here records in the order of the stages routed to it.
    [[nodiscard]] RenderPassId add_pass(const RenderPassDesc& desc);
    // Un-routes the pass's stages; Main cannot be removed. Throws Error for an unknown id.
    void remove_pass(RenderPassId id);
    // Throws Error for an unknown pass or when the routing would make a pass cover non-consecutive stages. RENDER_PASS_NONE skips the stage.
    void route_stage(RenderStage stage, RenderPassId id);
    [[nodiscard]] RenderPassId stage_pass(RenderStage stage) const { return m_route[static_cast<uint32_t>(stage)]; }
    [[nodiscard]] RenderPass& pass(RenderPassId id);
    // The routed passes in recording order, each once.
    [[nodiscard]] const std::vector<RenderPass*>& ordered_passes() const { return m_order; }
    void clear_items();

    // Throws Error while a scene is open, then recycles the batcher's pages for `frame_slot`.
    void finish_frame(uint32_t frame_slot);

    [[nodiscard]] BatchRenderer2D& batcher_2d() { return m_batcher; }

private:
    void run_stage(RenderStage stage, const RenderView& view);
    void rebuild_order();

    std::vector<UniquePtr<RenderPass>> m_passes;
    BatchRenderer2D m_batcher;
    DebugRenderer* m_debug;
    RenderPassId m_route[RENDER_STAGE_COUNT];
    std::vector<RenderPass*> m_order;
    struct Entry
    {
        RenderSource* source;
        SubmitContext context;
    };

    [[nodiscard]] bool whole(const Entry& entry) const;

    std::vector<Entry> m_sources;
    SubmitContext m_context;
    const Camera* m_camera = nullptr;
    Vec2f m_logical;
    Vec2f m_framebuffer;
    float m_scale = 1.0f;
    bool m_open = false;
};

// Sets the submit context for its lifetime and restores the previous one.
class SubmitScope
{
public:
    SubmitScope(SceneRenderer& scene, const SubmitContext& context)
        : m_scene(scene)
        , m_previous(scene.submit_context())
    {
        m_scene.set_submit_context(context);
    }
    ~SubmitScope() { m_scene.set_submit_context(m_previous); }

    SubmitScope(const SubmitScope&) = delete;
    SubmitScope& operator=(const SubmitScope&) = delete;

private:
    SceneRenderer& m_scene;
    SubmitContext m_previous;
};

// Opens a scene for its lifetime; the destructor ends it without throwing while another exception is unwinding.
class SceneScope
{
public:
    SceneScope(SceneRenderer& scene, const RenderView& view);
    ~SceneScope() noexcept(false);

    SceneScope(const SceneScope&) = delete;
    SceneScope& operator=(const SceneScope&) = delete;

private:
    SceneRenderer& m_scene;
};

} // namespace oryx
