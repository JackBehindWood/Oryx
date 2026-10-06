#include "oxpch.h"
#include "Oryx/Renderer/DefaultResources.h"

#include "Oryx/Graphics/Resources/Texture2D.h"

namespace oryx
{

DefaultResources create_default_resources(IRHI& rhi)
{
    const uint8_t white[4] = { 255, 255, 255, 255 };
    const Texture2D texture = Texture2D::create(rhi, { .width = 1, .height = 1, .pixels = white, .pixel_bytes = sizeof(white) });

    std::vector<uint16_t> indices(QUAD_INDEX_COUNT);
    for (uint32_t quad = 0; quad < QUAD_INDEX_MAX_QUADS; ++quad)
    {
        const uint16_t first = static_cast<uint16_t>(quad * 4);
        const uint16_t pattern[6] = { 0, 1, 2, 2, 3, 0 };
        for (uint32_t i = 0; i < 6; ++i)
        {
            indices[quad * 6 + i] = static_cast<uint16_t>(first + pattern[i]);
        }
    }
    const uint32_t bytes = static_cast<uint32_t>(indices.size() * sizeof(uint16_t));
    RHIBufferPtr quad_indices = rhi.create_buffer({ .size = bytes, .usage = RHIBufferUsage::Index, .memory = RHIMemory::GpuOnly, .initial_data = reinterpret_cast<const uint8_t*>(indices.data()), .initial_data_size = bytes, .name = "quad_indices" });
    return { texture.texture(), texture.sampler(), std::move(quad_indices) };
}

} // namespace oryx
