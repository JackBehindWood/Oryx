#pragma once

#include "Oryx/Renderer/BatchRenderer2D.h"
#include "Oryx/Renderer/Camera.h"

namespace oryx
{

// Scopes one 2D batcher scene: opens it on construction and closes it on destruction, so an exception cannot leave a scene open at the end of the frame.
// It opens no pass and ends no frame; those stay with the frame owner.
class ScreenScene
{
public:
    // The facade's batcher, so Renderer::draw_* land in this scene.
    explicit ScreenScene(const Camera& camera);
    ScreenScene(float width, float height);
    ScreenScene(BatchRenderer2D& batcher, const Camera& camera);

    // Throws Error when the scene was closed or nested wrongly, except while another exception is unwinding.
    ~ScreenScene() noexcept(false);

    ScreenScene(const ScreenScene&) = delete;
    ScreenScene& operator=(const ScreenScene&) = delete;

private:
    BatchRenderer2D& m_batcher;
};

} // namespace oryx
