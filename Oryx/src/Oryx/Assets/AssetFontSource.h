#pragma once

#include "Oryx/Assets/AssetManager.h"
#include "Oryx/Assets/GlyphAtlasBaker.h"
#include "Oryx/Text/IFontSource.h"

namespace oryx
{

struct FontSourceStats
{
    uint32_t bakes = 0;
    uint32_t rasterised = 0;
    uint32_t cache_hits = 0;
};

// Feeds a Font from a FontAsset: Ready once the asset is, revision follows the asset's reload revision, bakes through the AssetManager's cache.
// The manager must outlive the source.
class AssetFontSource final : public IFontSource
{
public:
    AssetFontSource(AssetManager& assets, AssetHandle<FontAsset> font)
        : m_assets(assets)
        , m_font(font)
    {
    }

    [[nodiscard]] bool ready() const override { return m_assets.try_get(m_font) != nullptr; }
    [[nodiscard]] uint32_t revision() const override { return m_assets.revision(m_font); }
    [[nodiscard]] GlyphAtlasData bake(const GlyphAtlasDesc& desc) override;

    [[nodiscard]] const FontSourceStats& stats() const { return m_stats; }

private:
    AssetManager& m_assets;
    AssetHandle<FontAsset> m_font;
    FontSourceStats m_stats;
};

[[nodiscard]] inline UniquePtr<IFontSource> make_font_source(AssetManager& assets, AssetHandle<FontAsset> font)
{
    return create_unique<AssetFontSource>(assets, font);
}

} // namespace oryx
