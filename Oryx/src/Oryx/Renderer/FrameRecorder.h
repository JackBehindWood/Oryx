#pragma once

#include "Oryx/Renderer/DefaultResources.h"
#include "Oryx/Renderer/DrawItem.h"
#include "Oryx/Renderer/GraphicsPipelineCache.h"

namespace oryx
{

class RHICommandList;

// Records one draw inside an open pass; throws Error for a missing or stale pipeline, a missing vertex buffer, or a binding the pipeline does not have.
// Texture-array slots the item leaves unset are bound to the default white texture, and an unset sampler to the default sampler, because backends need every element bound.
void record_draw_item(RHICommandList& commands, const DrawItem& item, const GraphicsPipelineCache& pipelines, const DefaultResources& defaults);

} // namespace oryx
