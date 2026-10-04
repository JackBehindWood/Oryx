#pragma once

#include "MetalDevice.h"
#include "MetalResources.h"

namespace oryx::metal
{

// Debug profiles cross-check the binding layout and vertex attributes against Metal's reflection and throw Error on a mismatch.
[[nodiscard]] Ref<MetalPipeline> create_metal_pipeline(const MetalDevice& device, const RHIGraphicsPipelineDesc& desc);

} // namespace oryx::metal
