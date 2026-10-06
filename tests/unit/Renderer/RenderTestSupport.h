#pragma once

#include "Oryx.h"

namespace oryx::test
{

// A source whose stages are a callback, for tests that draw a few primitives.
class FnSource final : public RenderSource
{
public:
    using Stage = std::function<void(RenderStage, StageContext&)>;

    explicit FnSource(Stage fn)
        : m_fn(std::move(fn))
    {
    }

    void render_stage(RenderStage stage, StageContext& context) override { m_fn(stage, context); }

private:
    Stage m_fn;
};

inline RenderView view_over(const Camera& camera, float width = 320.0f, float height = 200.0f)
{
    return { camera, { width, height }, { width, height }, 1.0f };
}

// Draws `draw` in the Scene2D stage of a one-source scene under `camera`.
inline void render_2d(const Camera& camera, const std::function<void(BatchRenderer2D&)>& draw)
{
    FnSource source([&draw](RenderStage stage, StageContext& context)
    {
        if (stage == RenderStage::Scene2D)
        {
            draw(context.batcher_2d);
        }
    });
    Renderer::scene().render(view_over(camera), source);
}

// Runs a windowed board's render inside one scene sized to the input's viewport, as GraphicsLayer would around a frame.
template<typename Board>
void render_board(Board& board, const BoardInput& input)
{
    const Camera2D camera = Camera2D::screen_space(input.viewport[0], input.viewport[1]);
    SceneRenderer& scene = Renderer::scene();
    scene.begin_scene(view_over(camera, input.viewport[0], input.viewport[1]));
    try
    {
        board.render(input);
    }
    catch (...)
    {
        scene.end_scene();
        throw;
    }
    scene.end_scene();
}

} // namespace oryx::test
