#pragma once

#include "Oryx/Graphics/Resources/Texture2D.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Math/Vector2.h"
#include "Oryx/Renderer/Batch/BatchRenderer.h"
#include "Oryx/Renderer/Font.h"
#include "Oryx/Renderer/Batch/Primitive2D.h"

namespace oryx
{

// The 2D primitive API. World space is left-handed (x right, y up, z into the screen); position is a primitive's centre and rotation (radians, counter-clockwise) turns it about that centre.
// A texture's uv rectangle runs from its top-left (uv_min) to its bottom-right (uv_max).
class BatchRenderer2D : public BatchRenderer
{
public:
    explicit BatchRenderer2D(const BatchRendererDesc& desc);

    void draw_triangle(const Vec2f& a, const Vec2f& b, const Vec2f& c, const Colour& colour);
    void draw_triangle(const Vec2f& a, const Vec2f& b, const Vec2f& c, const Colour& colour_a, const Colour& colour_b, const Colour& colour_c);
    void draw_line(const Vec2f& a, const Vec2f& b, const Colour& colour);
    void draw_line(const Vec2f& a, const Vec2f& b, const Colour& colour_a, const Colour& colour_b);
    // Corners run counter-clockwise from the bottom left.
    void draw_quad(const Vec2f (&corners)[4], const Colour& colour);
    void draw_rect(const Vec2f& position, const Vec2f& size, const Colour& colour, float rotation = 0.0f);
    void draw_sprite(const Vec2f& position, const Vec2f& size, const Texture2D& texture, const Colour& tint = { 1.0f, 1.0f, 1.0f, 1.0f }, float rotation = 0.0f, const Vec2f& uv_min = { 0.0f, 0.0f }, const Vec2f& uv_max = { 1.0f, 1.0f });
    // thickness 1 fills the disc and approaches 0 for a thin ring; fade softens the edge, both as fractions of the radius.
    void draw_circle(const Vec2f& centre, float radius, const Colour& colour, float thickness = 1.0f, float fade = 0.005f);

    // Draws UTF-8 text with `position` at the baseline start of the first line (y up, so '\n' moves down); a font that is not ready draws nothing. Throws Error outside a scene.
    void draw_text(const Vec2f& position, std::string_view text, Font& font, const TextStyle& style = {});

private:
    void write_quad(const Vec2f (&corners)[4], const RHITexturePtr& texture, const RHISamplerPtr& sampler, const Colour& colour, const Vec2f& uv_min, const Vec2f& uv_max);
    void write_glyph(const Vec2f (&corners)[4], const Texture2D& atlas_texture, float px_range, const Colour& colour, const Glyph& glyph);

    BatchStreamId m_streams[PRIMITIVE_2D_COUNT];
};

} // namespace oryx
