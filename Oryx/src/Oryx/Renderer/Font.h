#pragma once

#include "Oryx/Renderer/GlyphAtlas.h"
#include "Oryx/Text/IFontSource.h"
#include "Oryx/Text/TextLayout.h"

namespace oryx
{

inline constexpr uint32_t FONT_MAX_ATLASES = 8;

// What text is drawn with: a font source plus the glyph atlases baked from it, one per pixel height, created on first use and baked again when the source's revision changes.
// Callers never hold or poll an atlas. Main thread only.
class Font
{
public:
    [[nodiscard]] static Font create(UniquePtr<IFontSource> source, GlyphAtlasDesc desc = {});

    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;
    Font(Font&&) = default;
    Font& operator=(Font&&) = default;

    // The source is ready; text drawn before then is skipped.
    [[nodiscard]] bool ready() const { return m_source->ready(); }
    [[nodiscard]] uint32_t revision() const { return m_source->revision(); }

    // Zero while the font is not ready.
    [[nodiscard]] TextExtent measure(std::string_view text, float pixel_height, float scale = 1.0f);
    [[nodiscard]] float line_height(float pixel_height);
    [[nodiscard]] float ascent(float pixel_height);

    // The up-to-date atlas for the height (rounded to whole pixels), baked now if needed. Throws Error when not ready or the height is out of range.
    // The reference is valid until the next atlas call. The least recently used atlas is dropped beyond FONT_MAX_ATLASES.
    [[nodiscard]] GlyphAtlas& atlas(float pixel_height);
    void release_atlases();
    [[nodiscard]] size_t atlas_count() const { return m_atlases.size(); }

private:
    struct Entry
    {
        uint32_t pixel_height;
        uint32_t revision;
        uint64_t last_use;
        UniquePtr<GlyphAtlas> atlas;
    };

    Font(UniquePtr<IFontSource> source, GlyphAtlasDesc desc);

    UniquePtr<IFontSource> m_source;
    GlyphAtlasDesc m_desc;
    std::vector<Entry> m_atlases;
    uint64_t m_tick = 0;
};

} // namespace oryx
