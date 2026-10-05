#pragma once

#include "Oryx/Assets/Import/AssetCache.h"
#include "Oryx/Assets/Types/FontAsset.h"
#include "Oryx/Text/GlyphAtlasData.h"

namespace oryx
{

struct GlyphAtlasBake
{
    GlyphAtlasData data;
    uint32_t rasterised = 0;
    bool cache_hit = false;
};

// Rasterises the description's codepoints into a shelf-packed power-of-two R8 sheet with a gutter of `padding`, bakes the kerning of every glyph pair
// (stb_truetype reads only the `kern` table, so GPOS-only fonts yield none), and caches the result keyed by the font's content, the description and the format version.
// A corrupt or truncated cache entry is a miss. Throws Error for an invalid description or a sheet that does not fit.
[[nodiscard]] GlyphAtlasBake bake_glyph_atlas(const FontAsset& font, const GlyphAtlasDesc& desc, const AssetCache& cache);

} // namespace oryx
