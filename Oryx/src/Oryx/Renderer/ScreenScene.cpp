#include "oxpch.h"
#include "Oryx/Renderer/ScreenScene.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Renderer/Renderer.h"

namespace oryx
{

ScreenScene::ScreenScene(const Camera& camera)
    : ScreenScene(Renderer::batcher_2d(), camera)
{
}

ScreenScene::ScreenScene(float width, float height)
    : ScreenScene(Renderer::batcher_2d(), Camera2D::screen_space(width, height))
{
}

ScreenScene::ScreenScene(BatchRenderer2D& batcher, const Camera& camera)
    : m_batcher(batcher)
{
    m_batcher.begin(camera);
}

ScreenScene::~ScreenScene() noexcept(false)
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

} // namespace oryx
