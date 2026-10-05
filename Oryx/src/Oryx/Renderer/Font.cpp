#include "oxpch.h"
#include "Oryx/Renderer/Font.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

Font Font::create(UniquePtr<IFontSource> source, GlyphAtlasDesc desc)
{
    if (source == nullptr)
    {
        throw Error("Font needs a font source");
    }
    validate_glyph_atlas_desc(desc);
    return Font(std::move(source), std::move(desc));
}

Font::Font(UniquePtr<IFontSource> source, GlyphAtlasDesc desc)
    : m_source(std::move(source))
    , m_desc(std::move(desc))
{
}

GlyphAtlas& Font::atlas(float pixel_height)
{
    if (!(pixel_height > 0.0f) || pixel_height > GLYPH_ATLAS_MAX_PIXEL_HEIGHT)
    {
        throw Error("Font pixel height is out of range", std::to_string(pixel_height));
    }
    if (!ready())
    {
        throw Error("Font is not ready");
    }
    const uint32_t key = std::max(1u, static_cast<uint32_t>(pixel_height + 0.5f));
    const uint32_t revision = m_source->revision();
    ++m_tick;
    std::vector<Entry>::iterator found = std::find_if(m_atlases.begin(), m_atlases.end(), [key](const Entry& entry) { return entry.pixel_height == key; });
    if (found == m_atlases.end() || found->revision != revision)
    {
        GlyphAtlasDesc desc = m_desc;
        desc.pixel_height = static_cast<float>(key);
        UniquePtr<GlyphAtlas> baked = create_unique<GlyphAtlas>(m_source->bake(desc));
        if (found == m_atlases.end())
        {
            if (m_atlases.size() >= FONT_MAX_ATLASES)
            {
                m_atlases.erase(std::min_element(m_atlases.begin(), m_atlases.end(), [](const Entry& a, const Entry& b) { return a.last_use < b.last_use; }));
            }
            m_atlases.push_back({ key, revision, m_tick, std::move(baked) });
            found = m_atlases.end() - 1;
        }
        else
        {
            found->atlas = std::move(baked);
            found->revision = revision;
        }
    }
    found->last_use = m_tick;
    return *found->atlas;
}

TextExtent Font::measure(std::string_view text, float pixel_height, float scale)
{
    if (!ready())
    {
        return {};
    }
    return layout_text(atlas(pixel_height).data(), text, scale, [](const Glyph&, const Vec2f&) {});
}

float Font::line_height(float pixel_height)
{
    return ready() ? atlas(pixel_height).data().line_height() : 0.0f;
}

float Font::ascent(float pixel_height)
{
    return ready() ? atlas(pixel_height).data().ascent() : 0.0f;
}

void Font::release_atlases()
{
    m_atlases.clear();
}

} // namespace oryx
