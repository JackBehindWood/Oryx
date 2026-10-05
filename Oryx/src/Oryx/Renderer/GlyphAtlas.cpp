#include "oxpch.h"
#include "Oryx/Renderer/GlyphAtlas.h"

namespace oryx
{

const Texture2D& GlyphAtlas::texture(IRHI& rhi)
{
    if (m_texture == nullptr)
    {
        Texture2DDesc desc;
        desc.width = m_data.side();
        desc.height = m_data.side();
        desc.format = RHIFormat::R8Unorm;
        desc.pixels = m_data.pixels().data();
        desc.pixel_bytes = static_cast<uint32_t>(m_data.pixels().size());
        desc.filter = RHIFilter::Linear;
        desc.address = RHIAddressMode::Clamp;
        m_texture = create_unique<Texture2D>(Texture2D::create(rhi, desc));
    }
    return *m_texture;
}

} // namespace oryx
