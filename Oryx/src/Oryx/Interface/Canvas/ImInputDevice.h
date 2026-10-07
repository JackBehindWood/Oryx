#pragma once

#include "Oryx/Core/Input.h"
#include "Oryx/Interface/Canvas/ImInput.h"

namespace oryx
{

// One frame of ImInput from a window's input. The pointer is valid while the cursor is inside the surface; the wheel is the frame's accumulated scroll, passed through unscaled.
[[nodiscard]] ImInput make_im_input(const IInput& input, uint32_t surface, const Vec2f& surface_size, float scale, float delta_time);

} // namespace oryx
