#pragma once

#include "Oryx/Text/IFontSource.h"

namespace oryx
{

extern const uint8_t BUILTIN_FONT_DATA[];
extern const size_t BUILTIN_FONT_SIZE;

// The engine's own font: Inter embedded in the binary and rasterised on bake, always ready, so debug text needs no assets.
// Covers Basic Latin and Latin-1; kerning is not baked (the embedded subset has no `kern` table).
class BuiltinFontSource final : public IFontSource
{
public:
    [[nodiscard]] bool ready() const override { return true; }
    [[nodiscard]] uint32_t revision() const override { return 1; }
    [[nodiscard]] GlyphAtlasData bake(const GlyphAtlasDesc& desc) override;
};

} // namespace oryx
