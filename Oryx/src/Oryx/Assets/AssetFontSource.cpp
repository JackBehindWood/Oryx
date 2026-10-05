#include "oxpch.h"
#include "Oryx/Assets/AssetFontSource.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

GlyphAtlasData AssetFontSource::bake(const GlyphAtlasDesc& desc)
{
    const FontAsset* font = m_assets.try_get(m_font);
    if (font == nullptr)
    {
        throw Error("cannot bake glyphs: the font is not ready");
    }
    GlyphAtlasBake bake = bake_glyph_atlas(*font, desc, m_assets.cache());
    ++m_stats.bakes;
    m_stats.rasterised += bake.rasterised;
    m_stats.cache_hits += bake.cache_hit ? 1 : 0;
    return std::move(bake.data);
}

} // namespace oryx
