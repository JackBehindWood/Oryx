#pragma once

#include "Oryx/Text/GlyphAtlasData.h"

namespace oryx
{

// Where a Font gets its glyphs. The renderer knows only this interface; the asset system provides the implementation (AssetFontSource).
class IFontSource
{
public:
    virtual ~IFontSource() = default;

    // False while the font is loading or failed; bake may only be called when ready.
    [[nodiscard]] virtual bool ready() const = 0;
    // Changes whenever the underlying font is replaced (hot reload), so holders know to bake again.
    [[nodiscard]] virtual uint32_t revision() const = 0;
    // Throws Error when not ready or the description is invalid.
    [[nodiscard]] virtual GlyphAtlasData bake(const GlyphAtlasDesc& desc) = 0;
};

} // namespace oryx
