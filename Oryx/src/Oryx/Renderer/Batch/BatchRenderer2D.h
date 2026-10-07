#pragma once

#include "Oryx/Graphics/Resources/Texture2D.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Math/Vector2.h"
#include "Oryx/Renderer/Batch/BatchRenderer.h"
#include "Oryx/Renderer/Font.h"
#include "Oryx/Renderer/Batch/Primitive2D.h"
#include "Oryx/Renderer/Batch/Vertex2D.h"

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
    // Fills the convex polygon `points` (y up) as a fan from the centre of the box (`position`, `size`); the texture's uv rectangle is mapped linearly over that box, so the polygon shows the part of the image under it.
    void draw_sprite_polygon(const Vec2f& position, const Vec2f& size, const Vec2f* points, uint32_t count, const Texture2D& texture, const Colour& tint = { 1.0f, 1.0f, 1.0f, 1.0f }, const Vec2f& uv_min = { 0.0f, 0.0f }, const Vec2f& uv_max = { 1.0f, 1.0f });
    // thickness 1 fills the disc and approaches 0 for a thin ring; fade softens the edge, both as fractions of the radius.
    void draw_circle(const Vec2f& centre, float radius, const Colour& colour, float thickness = 1.0f, float fade = 0.005f);

    // Draws UTF-8 text with `position` at the baseline start of the first line (y up, so '\n' moves down); a font that is not ready draws nothing. Throws Error outside a scene.
    void draw_text(const Vec2f& position, std::string_view text, Font& font, const TextStyle& style = {});

    // The GUI path: one stream and pipeline for everything, so these never break a batch against each other. Corners run counter-clockwise from the bottom left.
    void draw_ui_quad(const Vec2f (&corners)[4], const Colour& colour);
    void draw_ui_image(const Vec2f& position, const Vec2f& size, const Texture2D& texture, const Colour& tint, const Vec2f& uv_min, const Vec2f& uv_max);
    void draw_ui_text(const Vec2f& position, std::string_view text, Font& font, const TextStyle& style = {});
    // Radii run top-left, top-right, bottom-right, bottom-left and are clamped to the shorter half-extent; the edge is anti-aliased and the outline sits inside the box.
    void draw_ui_rounded(const Vec2f& position, const Vec2f& size, const float (&radii)[4], const Colour& colour);
    void draw_ui_border(const Vec2f& position, const Vec2f& size, const float (&radii)[4], float thickness, const Colour& colour);

    // Clips later draws to a world-space rect (centre and size, as draw_rect), intersected with the enclosing clip and the framebuffer; the rect resolves to whole
    // framebuffer pixels, rounded outward. An empty result culls draws until the matching pop_clip. Throws Error outside a scene or before the framebuffer size is known.
    void push_clip(const Vec2f& position, const Vec2f& size);
    // Throws Error with no clip pushed.
    void pop_clip();
    [[nodiscard]] uint32_t clip_depth() const { return static_cast<uint32_t>(m_clips.size()); }
    // The active clip in framebuffer pixels, top left origin; meaningful only while clip_depth() > 0.
    [[nodiscard]] const RHIScissorRect& clip_scissor() const { return m_clips.back().pixels; }
    // True when a rect (centre and size) lies fully outside the active clip, so callers can skip emitting it; false with no clip.
    [[nodiscard]] bool is_clipped(const Vec2f& position, const Vec2f& size) const;

protected:
    void on_begin() override { m_clips.clear(); }

private:
    struct ClipEntry
    {
        RHIScissorRect pixels;
    };

    [[nodiscard]] RHIScissorRect to_pixels(const Vec2f& position, const Vec2f& size) const;
    [[nodiscard]] bool culled() const { return !m_clips.empty() && (m_clips.back().pixels.width == 0 || m_clips.back().pixels.height == 0); }

    void write_quad(const Vec2f (&corners)[4], const RHITexturePtr& texture, const RHISamplerPtr& sampler, const Colour& colour, const Vec2f& uv_min, const Vec2f& uv_max);
    void write_quad_uvs(const Vec2f (&corners)[4], const Vec2f (&uvs)[4], const RHITexturePtr& texture, const RHISamplerPtr& sampler, const Colour& colour);
    struct UiPrimitive
    {
        const RHITexturePtr* texture = nullptr;
        const RHISamplerPtr* sampler = nullptr;
        UiMode mode = UiMode::Texture;
        float thickness = 0.0f;
        float px_range = 0.0f;
        float radii[4] = {};
    };

    void write_ui(const Vec2f (&corners)[4], const Vec2f (&uvs)[4], const Colour& colour, const UiPrimitive& primitive);
    void write_ui_shape(const Vec2f& position, const Vec2f& size, const float (&radii)[4], float thickness, UiMode mode, const Colour& colour);
    void write_glyph(const Vec2f (&corners)[4], const Texture2D& atlas_texture, float px_range, const Colour& colour, const Glyph& glyph);

    BatchStreamId m_streams[PRIMITIVE_2D_COUNT];
    std::vector<ClipEntry> m_clips;
};

// Pushes a clip for its lifetime.
class ClipScope
{
public:
    ClipScope(BatchRenderer2D& batcher, const Vec2f& position, const Vec2f& size)
        : m_batcher(batcher)
    {
        m_batcher.push_clip(position, size);
    }
    ~ClipScope() { m_batcher.pop_clip(); }

    ClipScope(const ClipScope&) = delete;
    ClipScope& operator=(const ClipScope&) = delete;

private:
    BatchRenderer2D& m_batcher;
};

} // namespace oryx
