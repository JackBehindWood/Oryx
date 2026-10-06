#pragma once

#include "Oryx/Core/Input.h"
#include "Oryx/Math/Vector2.h"

namespace oryx
{

// One frame as GraphicsLayer sees it: the window's input and sizes. Positions are logical window points with the origin top left;
// `framebuffer` is in pixels and equals `logical * scale`.
struct FrameInfo
{
    const IInput& input;
    Vec2f logical;
    Vec2f framebuffer;
    float scale = 1.0f;
    double delta_time = 0.0;
};

// Something that draws into, or reads input for, each frame; GraphicsLayer calls it once per frame before it ends the frame.
class IFrameClient
{
public:
    virtual ~IFrameClient() = default;

    virtual void frame(const FrameInfo& info) = 0;
};

} // namespace oryx
