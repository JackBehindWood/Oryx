#pragma once

#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Graphics/Resources/Texture2D.h"
#include "Oryx/Text/GlyphAtlasData.h"

namespace oryx
{

// A baked glyph sheet on the GPU: the CPU data plus a texture uploaded the first time a device asks for it, so the atlas holds no device.
// Immutable once built; a reloaded font produces a new atlas. Main thread only.
class GlyphAtlas
{
public:
    explicit GlyphAtlas(GlyphAtlasData data)
        : m_data(std::move(data))
    {
    }

    [[nodiscard]] const GlyphAtlasData& data() const { return m_data; }
    // The atlas is tied to the first device that uploads it.
    [[nodiscard]] const Texture2D& texture(IRHI& rhi);

private:
    GlyphAtlasData m_data;
    UniquePtr<Texture2D> m_texture;
};

} // namespace oryx
