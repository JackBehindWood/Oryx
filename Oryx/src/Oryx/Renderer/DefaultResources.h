#pragma once

#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

inline constexpr uint32_t QUAD_INDEX_MAX_QUADS = 16384;
inline constexpr uint32_t QUAD_INDEX_COUNT = QUAD_INDEX_MAX_QUADS * 6;

// Shared fallbacks the frame recorder binds where a draw leaves a texture-array slot or the sampler unset.
struct DefaultResources
{
    RHITexturePtr white_texture;
    RHISamplerPtr sampler;
    // U16 indices 0,1,2,2,3,0 repeated per quad (four vertices each), GpuOnly; every quad and circle batch draws from it.
    RHIBufferPtr quad_indices;
};

[[nodiscard]] DefaultResources create_default_resources(IRHI& rhi);

} // namespace oryx
