#include "oxpch.h"
#include "Oryx/Assets/FontAsset.h"

#include "Oryx/Assets/AssetFile.h"
#include "Oryx/Assets/AssetManager.h"
#include "Oryx/Core/Error.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace oryx
{

struct FontAsset::Impl
{
    std::vector<uint8_t> bytes;
    stbtt_fontinfo info{};
};

FontAsset::FontAsset(UniquePtr<Impl> impl)
    : m_impl(std::move(impl))
{
}

FontAsset::FontAsset(FontAsset&&) noexcept = default;
FontAsset& FontAsset::operator=(FontAsset&&) noexcept = default;
FontAsset::~FontAsset() = default;

FontAsset FontAsset::from_bytes(std::vector<uint8_t> bytes)
{
    UniquePtr<Impl> impl = create_unique<Impl>();
    impl->bytes = std::move(bytes);
    const uint8_t* data = impl->bytes.data();
    int32_t offset = impl->bytes.empty() ? -1 : stbtt_GetFontOffsetForIndex(data, 0);
    if (offset < 0 || stbtt_InitFont(&impl->info, data, offset) == 0)
    {
        throw Error("cannot parse font");
    }
    return FontAsset(std::move(impl));
}

bool FontAsset::has_glyph(uint32_t codepoint) const
{
    return stbtt_FindGlyphIndex(&m_impl->info, static_cast<int32_t>(codepoint)) != 0;
}

FontMetrics FontAsset::metrics(float pixel_height) const
{
    float scale = stbtt_ScaleForPixelHeight(&m_impl->info, pixel_height);
    int32_t ascent = 0;
    int32_t descent = 0;
    int32_t line_gap = 0;
    stbtt_GetFontVMetrics(&m_impl->info, &ascent, &descent, &line_gap);
    return FontMetrics{ static_cast<float>(ascent) * scale, static_cast<float>(descent) * scale, static_cast<float>(line_gap) * scale };
}

GlyphBitmap FontAsset::rasterise(uint32_t codepoint, float pixel_height) const
{
    float scale = stbtt_ScaleForPixelHeight(&m_impl->info, pixel_height);
    int32_t advance = 0;
    int32_t left_bearing = 0;
    stbtt_GetCodepointHMetrics(&m_impl->info, static_cast<int32_t>(codepoint), &advance, &left_bearing);

    GlyphBitmap glyph;
    glyph.advance = static_cast<float>(advance) * scale;

    int32_t width = 0;
    int32_t height = 0;
    int32_t x_offset = 0;
    int32_t y_offset = 0;
    uint8_t* bitmap = stbtt_GetCodepointBitmap(&m_impl->info, scale, scale, static_cast<int32_t>(codepoint), &width, &height, &x_offset, &y_offset);
    if (bitmap == nullptr)
    {
        return glyph;
    }
    glyph.width = width;
    glyph.height = height;
    glyph.x_offset = x_offset;
    glyph.y_offset = y_offset;
    glyph.coverage.assign(bitmap, bitmap + static_cast<size_t>(width) * static_cast<size_t>(height));
    stbtt_FreeBitmap(bitmap, nullptr);
    return glyph;
}

namespace
{

class FontAssetLoader : public IAssetLoader<FontAsset>
{
public:
    UniquePtr<FontAsset> load(const std::filesystem::path& path) const override
    {
        return create_unique<FontAsset>(FontAsset::from_bytes(read_binary_file(path)));
    }
};

} // namespace

OX_REGISTER_ASSET_LOADER(FontAsset, FontAssetLoader, ".ttf")

} // namespace oryx
