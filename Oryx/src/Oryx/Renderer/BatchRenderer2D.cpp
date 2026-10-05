#include "oxpch.h"
#include "Oryx/Renderer/BatchRenderer2D.h"

#include "Oryx/Renderer/Vertex2D.h"

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
        m_streams[i] = register_stream({ traits.vertex_size, traits.vertices_per_primitive, traits.indices_per_primitive, traits.pipeline == BuiltinPipeline::Quad, traits.pipeline });
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

void BatchRenderer2D::write_quad(const Vec2f (&corners)[4], const RHITexturePtr& texture, const RHISamplerPtr& sampler, const Colour& colour, const Vec2f& uv_min, const Vec2f& uv_max)
{
    select(m_streams[static_cast<uint32_t>(Primitive2D::Quad)], sampler);
    const float slot = static_cast<float>(acquire_texture(texture));
    const Vec2f uvs[4] = { { uv_min[0], uv_max[1] }, { uv_max[0], uv_max[1] }, { uv_max[0], uv_min[1] }, { uv_min[0], uv_min[1] } };
    Vertex2DQuad vertices[4];
    for (uint32_t i = 0; i < 4; ++i)
    {
        vertices[i] = { base_vertex(corners[i], colour), uvs[i], slot };
    }
    write(append(), vertices);
}

} // namespace oryx
