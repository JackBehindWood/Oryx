#pragma once

#include "Oryx/Interface/Canvas/DrawList.h"
#include "Oryx/Renderer/Batch/BatchRenderer2D.h"

namespace oryx
{

// Where a list lands. The list's origin sits at `origin` (logical points from the window's top-left) in a window `window` points tall; the batcher's camera is the screen-space one (origin bottom-left, y up).
struct ReplayTarget
{
    Vec2f origin{ 0.0f, 0.0f };
    Vec2f window{ 0.0f, 0.0f };
};

// Emits every channel in order through the open scene of `batcher`, flipping y once. The batcher keeps the clip set on return.
void replay(const DrawList& list, BatchRenderer2D& batcher, Font& font, const ReplayTarget& target);

} // namespace oryx
