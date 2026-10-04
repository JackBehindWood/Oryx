#include "oxpch.h"
#include "Oryx/Renderer/DefaultResources.h"

#include "Oryx/Graphics/Resources/Texture2D.h"

namespace oryx
{

DefaultResources create_default_resources(IRHI& rhi)
{
    const uint8_t white[4] = { 255, 255, 255, 255 };
    const Texture2D texture = Texture2D::create(rhi, { .width = 1, .height = 1, .pixels = white, .pixel_bytes = sizeof(white) });
    return { texture.texture(), texture.sampler() };
}

} // namespace oryx
