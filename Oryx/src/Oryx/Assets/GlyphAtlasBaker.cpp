#include "oxpch.h"
#include "Oryx/Assets/GlyphAtlasBaker.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Fnv.h"

namespace oryx
{

namespace
{

constexpr uint32_t ATLAS_FORMAT_VERSION = 1;
constexpr const char* ATLAS_TYPE = "atlas";
constexpr uint32_t MIN_ATLAS_SIDE = 64;
constexpr uint32_t MAX_ATLAS_SIDE = 8192;

struct PayloadHeader
{
    uint32_t side;
    uint32_t glyph_count;
    uint32_t kerning_count;
    float ascent;
    float line_height;
};

struct Rasterised
{
    uint32_t codepoint = 0;
    GlyphBitmap bitmap;
};

struct Placement
{
    uint32_t x = 0;
    uint32_t y = 0;
};

uint64_t settings_hash(const GlyphAtlasDesc& desc)
{
    Fnv1a hash;
    uint32_t height_bits = 0;
    std::memcpy(&height_bits, &desc.pixel_height, sizeof(height_bits));
    hash.mix_value(height_bits);
    hash.mix_value(desc.padding);
    hash.mix_value(static_cast<uint64_t>(desc.encoding));
    for (const CodepointRange& range : desc.ranges)
    {
        hash.mix_value(range.first);
        hash.mix_value(range.last);
    }
    return hash.value();
}

bool place(const std::vector<Rasterised>& glyphs, const std::vector<uint32_t>& order, uint32_t side, uint32_t padding, std::vector<Placement>& placements)
{
    uint32_t x = padding;
    uint32_t y = padding;
    uint32_t row_height = 0;
    for (uint32_t index : order)
    {
        const GlyphBitmap& bitmap = glyphs[index].bitmap;
        if (bitmap.width <= 0 || bitmap.height <= 0)
        {
            continue;
        }
        const uint32_t width = static_cast<uint32_t>(bitmap.width);
        const uint32_t height = static_cast<uint32_t>(bitmap.height);
        if (x + width + padding > side)
        {
            x = padding;
            y += row_height + padding;
            row_height = 0;
        }
        if (width + 2 * padding > side || y + height + padding > side)
        {
            return false;
        }
        placements[index] = { x, y };
        x += width + padding;
        row_height = std::max(row_height, height);
    }
    return true;
}

template<typename T>
void append_array(std::vector<uint8_t>& out, const T* data, size_t count)
{
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data);
    out.insert(out.end(), bytes, bytes + count * sizeof(T));
}

template<typename T>
bool read_array(const std::vector<uint8_t>& payload, size_t& offset, T* out, size_t count)
{
    const size_t bytes = count * sizeof(T);
    if (count > payload.size() || bytes > payload.size() - offset)
    {
        return false;
    }
    if (bytes > 0)
    {
        std::memcpy(out, payload.data() + offset, bytes);
    }
    offset += bytes;
    return true;
}

std::vector<uint8_t> serialise(uint32_t side, float ascent, float line_height, const std::vector<GlyphEntry>& glyphs, const std::vector<KerningPair>& kerning, const std::vector<uint8_t>& pixels)
{
    const PayloadHeader header = { side, static_cast<uint32_t>(glyphs.size()), static_cast<uint32_t>(kerning.size()), ascent, line_height };
    std::vector<uint8_t> payload;
    payload.reserve(sizeof(header) + glyphs.size() * sizeof(GlyphEntry) + kerning.size() * sizeof(KerningPair) + pixels.size());
    append_array(payload, &header, 1);
    append_array(payload, glyphs.data(), glyphs.size());
    append_array(payload, kerning.data(), kerning.size());
    append_array(payload, pixels.data(), pixels.size());
    return payload;
}

bool deserialise_atlas(const std::vector<uint8_t>& payload, uint32_t& side, float& ascent, float& line_height, std::vector<GlyphEntry>& glyphs, std::vector<KerningPair>& kerning, std::vector<uint8_t>& pixels)
{
    PayloadHeader header = {};
    size_t offset = 0;
    if (!read_array(payload, offset, &header, 1))
    {
        return false;
    }
    const bool power_of_two = header.side >= MIN_ATLAS_SIDE && header.side <= MAX_ATLAS_SIDE && (header.side & (header.side - 1)) == 0;
    if (!power_of_two || header.glyph_count > GLYPH_ATLAS_MAX_GLYPHS || header.kerning_count > GLYPH_ATLAS_MAX_GLYPHS * GLYPH_ATLAS_MAX_GLYPHS)
    {
        return false;
    }
    const size_t expected = sizeof(header) + header.glyph_count * sizeof(GlyphEntry) + header.kerning_count * sizeof(KerningPair) + static_cast<size_t>(header.side) * header.side;
    if (payload.size() != expected)
    {
        return false;
    }
    glyphs.resize(header.glyph_count);
    kerning.resize(header.kerning_count);
    pixels.resize(static_cast<size_t>(header.side) * header.side);
    if (!read_array(payload, offset, glyphs.data(), glyphs.size()) || !read_array(payload, offset, kerning.data(), kerning.size()) || !read_array(payload, offset, pixels.data(), pixels.size()))
    {
        return false;
    }
    side = header.side;
    ascent = header.ascent;
    line_height = header.line_height;
    return true;
}

} // namespace

OX_REGISTER_COMPILED_TYPE(ATLAS_TYPE, ATLAS_FORMAT_VERSION)

GlyphAtlasBake bake_glyph_atlas(const FontAsset& font, const GlyphAtlasDesc& desc, const CompiledAssetStore& store)
{
    validate_glyph_atlas_desc(desc);
    const uint64_t content = font.content_hash();
    const CompiledAssetKey key = make_compiled_asset_key(ATLAS_TYPE, ATLAS_FORMAT_VERSION, settings_hash(desc), reinterpret_cast<const uint8_t*>(&content), sizeof(content));

    uint32_t side = 0;
    float ascent = 0.0f;
    float line_height = 0.0f;
    std::vector<GlyphEntry> glyphs;
    std::vector<KerningPair> kerning;
    std::vector<uint8_t> pixels;
    uint32_t rasterised_count = 0;
    bool cache_hit = false;

    std::vector<uint8_t> payload;
    if (store.read(key, payload) && deserialise_atlas(payload, side, ascent, line_height, glyphs, kerning, pixels))
    {
        cache_hit = true;
    }
    else
    {
        const std::vector<uint32_t> candidates = glyph_atlas_codepoints(desc);
        std::vector<Rasterised> rasterised;
        for (uint32_t codepoint : candidates)
        {
            if (font.has_glyph(codepoint))
            {
                rasterised.push_back({ codepoint, font.rasterise(codepoint, desc.pixel_height) });
                ++rasterised_count;
            }
        }

        std::vector<uint32_t> order(rasterised.size());
        for (uint32_t i = 0; i < order.size(); ++i)
        {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(), [&rasterised](uint32_t a, uint32_t b) { return rasterised[a].bitmap.height > rasterised[b].bitmap.height; });

        std::vector<Placement> placements(rasterised.size());
        side = MIN_ATLAS_SIDE;
        while (!place(rasterised, order, side, desc.padding, placements))
        {
            if (side >= MAX_ATLAS_SIDE)
            {
                throw Error("GlyphAtlas does not fit", "reduce the pixel height or the codepoint ranges");
            }
            side *= 2;
        }

        pixels.assign(static_cast<size_t>(side) * side, 0);
        const float inverse = 1.0f / static_cast<float>(side);
        for (uint32_t i = 0; i < rasterised.size(); ++i)
        {
            const GlyphBitmap& bitmap = rasterised[i].bitmap;
            Glyph glyph;
            glyph.advance = bitmap.advance;
            if (bitmap.width > 0 && bitmap.height > 0)
            {
                const uint32_t width = static_cast<uint32_t>(bitmap.width);
                const uint32_t height = static_cast<uint32_t>(bitmap.height);
                for (uint32_t row = 0; row < height; ++row)
                {
                    std::memcpy(&pixels[static_cast<size_t>(placements[i].y + row) * side + placements[i].x], &bitmap.coverage[static_cast<size_t>(row) * width], width);
                }
                glyph.uv_min = Vec2f(static_cast<float>(placements[i].x) * inverse, static_cast<float>(placements[i].y) * inverse);
                glyph.uv_max = Vec2f(static_cast<float>(placements[i].x + width) * inverse, static_cast<float>(placements[i].y + height) * inverse);
                glyph.size = Vec2f(static_cast<float>(width), static_cast<float>(height));
                glyph.bearing = Vec2f(static_cast<float>(bitmap.x_offset), -static_cast<float>(bitmap.y_offset + bitmap.height));
            }
            glyphs.push_back({ rasterised[i].codepoint, glyph });
        }

        for (const GlyphEntry& left : glyphs)
        {
            for (const GlyphEntry& right : glyphs)
            {
                const float advance = font.kerning(left.codepoint, right.codepoint, desc.pixel_height);
                if (advance != 0.0f)
                {
                    kerning.push_back({ left.codepoint, right.codepoint, advance });
                }
            }
        }

        const FontMetrics metrics = font.metrics(desc.pixel_height);
        ascent = metrics.ascent;
        line_height = metrics.ascent - metrics.descent + metrics.line_gap;
        const std::vector<uint8_t> bytes = serialise(side, ascent, line_height, glyphs, kerning, pixels);
        store.write(key, bytes.data(), bytes.size());
    }

    return { GlyphAtlasData(desc, side, std::move(pixels), std::move(glyphs), std::move(kerning), ascent, line_height), rasterised_count, cache_hit };
}

} // namespace oryx
