#pragma once

#include "Oryx/Math/Vector2.h"
#include "Oryx/Renderer/FrameClient.h"
#include "Oryx/Renderer/Scene/Camera.h"

namespace oryx
{

// How a frame is looked at, apart from what is drawn: the camera plus the window's sizes. It holds no game data, so any scene (board or not, 2D or 3D) renders through the same view type.
// The camera must outlive the scene it opens.
struct RenderView
{
    const Camera& camera;
    Vec2f logical;
    Vec2f framebuffer;
    float scale = 1.0f;
};

[[nodiscard]] inline RenderView make_render_view(const Camera& camera, const FrameInfo& info)
{
    return { camera, info.logical, info.framebuffer, info.scale };
}

} // namespace oryx
