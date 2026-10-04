#pragma once

#include "Oryx/Graphics/RHI/IRHI.h"

namespace oryx
{

// Shared fallbacks the frame recorder binds where a draw leaves a texture-array slot or the sampler unset.
struct DefaultResources
{
    RHITexturePtr white_texture;
    RHISamplerPtr sampler;
};

[[nodiscard]] DefaultResources create_default_resources(IRHI& rhi);

} // namespace oryx
