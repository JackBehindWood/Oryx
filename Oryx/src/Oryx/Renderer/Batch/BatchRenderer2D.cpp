#include "oxpch.h"
#include "Oryx/Renderer/Batch/BatchRenderer2D.h"

#include "Oryx/Renderer/Batch/Vertex2D.h"

namespace oryx
{

namespace
{

Vec4f to_vec4(const Colour& colour)
{
    return Vec4f(colour.r, colour.g, colour.b, colour.a);
}

Vertex2DBase base_vertex(const Vec2f& position, const Colour& colour)
{
    return { Vec3f(position[0], position[1], 0.0f), to_vec4(colour) };
}

void rect_corners(const Vec2f& position, const Vec2f& size, float rotation, Vec2f (&corners)[4])
{
    const float c = math::cos(rotation);
    const float s = math::sin(rotation);
    const float hx = size[0] * 0.5f;
    const float hy = size[1] * 0.5f;
    const Vec2f local[4] = { { -hx, -hy }, { hx, -hy }, { hx, hy }, { -hx, hy } };
    for (uint32_t i = 0; i < 4; ++i)
    {
        corners[i] = Vec2f(position[0] + c * local[i][0] - s * local[i][1], position[1] + s * local[i][0] + c * local[i][1]);
    }
}

void quad_uvs(const Vec2f& uv_min, const Vec2f& uv_max, Vec2f (&uvs)[4])
{
    uvs[0] = { uv_min[0], uv_max[1] };
    uvs[1] = { uv_max[0], uv_max[1] };
    uvs[2] = { uv_max[0], uv_min[1] };
    uvs[3] = { uv_min[0], uv_min[1] };
}

template<typename T, size_t N>
void write(uint8_t* destination, const T (&vertices)[N])
{
    std::memcpy(destination, vertices, sizeof(vertices));
}

} // namespace

BatchRenderer2D::BatchRenderer2D(const BatchRendererDesc& desc)
    : BatchRenderer(desc)
{
    for (uint32_t i = 0; i < PRIMITIVE_2D_COUNT; ++i)
    {
        const PrimitiveTraits traits = primitive_traits(static_cast<Primitive2D>(i));
        uint32_t triangles = 0;
        if (traits.topology == RHITopology::Triangles)
        {
            triangles = (traits.indexed ? traits.indices_per_primitive : traits.vertices_per_primitive) / 3;
        }
        m_streams[i] = register_stream({ traits.vertex_size, traits.vertices_per_primitive, traits.indices_per_primitive, traits.textured, &pipeline_def(static_cast<Primitive2D>(i)), triangles });
    }
}

void BatchRenderer2D::draw_triangle(const Vec2f& a, const Vec2f& b, const Vec2f& c, const Colour& colour)
{
    draw_triangle(a, b, c, colour, colour, colour);
}

void BatchRenderer2D::draw_triangle(const Vec2f& a, const Vec2f& b, const Vec2f& c, const Colour& colour_a, const Colour& colour_b, const Colour& colour_c)
{
    select(m_streams[static_cast<uint32_t>(Primitive2D::Triangle)], {});
    const Vertex2DLine vertices[3] = { { base_vertex(a, colour_a) }, { base_vertex(b, colour_b) }, { base_vertex(c, colour_c) } };
    write(append(), vertices);
}

void BatchRenderer2D::draw_line(const Vec2f& a, const Vec2f& b, const Colour& colour)
{
    draw_line(a, b, colour, colour);
}

void BatchRenderer2D::draw_line(const Vec2f& a, const Vec2f& b, const Colour& colour_a, const Colour& colour_b)
{
    select(m_streams[static_cast<uint32_t>(Primitive2D::Line)], {});
    const Vertex2DLine vertices[2] = { { base_vertex(a, colour_a) }, { base_vertex(b, colour_b) } };
    write(append(), vertices);
}

void BatchRenderer2D::draw_quad(const Vec2f (&corners)[4], const Colour& colour)
{
    write_quad(corners, defaults().white_texture, defaults().sampler, colour, { 0.0f, 0.0f }, { 1.0f, 1.0f });
}

void BatchRenderer2D::draw_rect(const Vec2f& position, const Vec2f& size, const Colour& colour, float rotation)
{
    Vec2f corners[4];
    rect_corners(position, size, rotation, corners);
    draw_quad(corners, colour);
}

void BatchRenderer2D::draw_sprite(const Vec2f& position, const Vec2f& size, const Texture2D& texture, const Colour& tint, float rotation, const Vec2f& uv_min, const Vec2f& uv_max)
{
    Vec2f corners[4];
    rect_corners(position, size, rotation, corners);
    write_quad(corners, texture.texture(), texture.sampler(), tint, uv_min, uv_max);
}

void BatchRenderer2D::draw_circle(const Vec2f& centre, float radius, const Colour& colour, float thickness, float fade)
{
    Vec2f corners[4];
    rect_corners(centre, { radius * 2.0f, radius * 2.0f }, 0.0f, corners);
    const Vec2f local[4] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
    select(m_streams[static_cast<uint32_t>(Primitive2D::Circle)], {});
    Vertex2DCircle vertices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        vertices[i] = { base_vertex(corners[i], colour), local[i], thickness, fade };
    }
    write(append(), vertices);
}

void BatchRenderer2D::draw_text(const Vec2f& position, std::string_view text, Font& font, const TextStyle& style)
{
    require_open();
    if (!font.ready())
    {
        return;
    }
    GlyphAtlas& atlas = font.atlas(style.pixel_height);
    const Texture2D& atlas_texture = atlas.texture(rhi());
    float align_offset = 0.0f;
    if (style.align != TextAlign::Left)
    {
        TextExtent extent = layout_text(atlas.data(), text, style.scale, [](const Glyph&, const Vec2f&) {});
        align_offset = style.align == TextAlign::Centre ? -0.5f * extent.width : -extent.width;
    }
    layout_text(atlas.data(), text, style.scale, [&](const Glyph& glyph, const Vec2f& pen) {
        if (glyph.size[0] <= 0.0f || glyph.size[1] <= 0.0f)
        {
            return;
        }
        const float x0 = position[0] + align_offset + pen[0] + glyph.bearing[0] * style.scale;
        const float y0 = position[1] + pen[1] + glyph.bearing[1] * style.scale;
        const float x1 = x0 + glyph.size[0] * style.scale;
        const float y1 = y0 + glyph.size[1] * style.scale;
        const Vec2f corners[4] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
        write_glyph(corners, atlas_texture, atlas.data().px_range(), style.colour, glyph);
    });
}

void BatchRenderer2D::write_glyph(const Vec2f (&corners)[4], const Texture2D& atlas_texture, float px_range, const Colour& colour, const Glyph& glyph)
{
    select(m_streams[static_cast<uint32_t>(Primitive2D::Text)], atlas_texture.sampler());
    const float slot = static_cast<float>(acquire_texture(atlas_texture.texture()));
    Vec2f uvs[4];
    quad_uvs(glyph.uv_min, glyph.uv_max, uvs);
    Vertex2DText vertices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        vertices[i] = { base_vertex(corners[i], colour), uvs[i], slot, px_range };
    }
    write(append(), vertices);
}

void BatchRenderer2D::write_quad(const Vec2f (&corners)[4], const RHITexturePtr& texture, const RHISamplerPtr& sampler, const Colour& colour, const Vec2f& uv_min, const Vec2f& uv_max)
{
    select(m_streams[static_cast<uint32_t>(Primitive2D::Quad)], sampler);
    const float slot = static_cast<float>(acquire_texture(texture));
    Vec2f uvs[4];
    quad_uvs(uv_min, uv_max, uvs);
    Vertex2DQuad vertices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        vertices[i] = { base_vertex(corners[i], colour), uvs[i], slot };
    }
    write(append(), vertices);
}

} // namespace oryx
