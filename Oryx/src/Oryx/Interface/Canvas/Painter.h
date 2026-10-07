#pragma once

#include "Oryx/Interface/Canvas/DrawList.h"
#include "Oryx/Interface/Canvas/Icon.h"
#include "Oryx/Renderer/Font.h"

namespace oryx
{

struct TextSpec
{
    float pixel_height = 16.0f;
    Colour colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    TextAlign align = TextAlign::Left;
    // Cuts overlong text to fit with "..."; otherwise it is clipped to the box.
    bool ellipsis = false;
};

struct ImageSpec
{
    Vec2f uv_min{ 0.0f, 0.0f };
    Vec2f uv_max{ 1.0f, 1.0f };
    CornerRadius radius;
    Colour tint = { 1.0f, 1.0f, 1.0f, 1.0f };
};

static_assert(std::is_trivially_copyable_v<ImageSpec> && std::is_standard_layout_v<ImageSpec>);

// The recording front end of a DrawList: snaps to device pixels and lays text out in boxes. Cheap to construct per use; holds no state of its own.
class Painter
{
public:
    // `scale` is device pixels per logical point.
    Painter(DrawList& list, Font& font, float scale = 1.0f)
        : m_list(list)
        , m_font(font)
        , m_scale(scale)
    {
    }

    [[nodiscard]] DrawList& list() { return m_list; }
    [[nodiscard]] float scale() const { return m_scale; }

    void fill_rect(const Rect& rect, const Colour& colour);
    void fill_rounded_rect(const Rect& rect, const CornerRadius& radius, const Colour& colour);
    void border(const Rect& rect, const CornerRadius& radius, float thickness, const Colour& colour);
    // A picture of the replay's image table; the rect snaps like the fills. A radius rounds its corners.
    void image(const Rect& rect, ImageHandle image, const ImageSpec& spec = {});
    void line(const Vec2f& from, const Vec2f& to, const Colour& colour, float thickness = 1.0f);
    // A chevron, cross, plus, minus or check centred in `box`; see draw_icon.
    void icon(const Rect& box, Icon icon, const Colour& colour, float size = 0.0f, float thickness = 1.5f);
    // Draws one line of text centred vertically in `box` and placed horizontally by `spec.align`. Skipped while the font is not ready.
    void text(const Rect& box, std::string_view text, const TextSpec& spec = {});

    [[nodiscard]] float text_width(std::string_view text, float pixel_height);

private:
    DrawList& m_list;
    Font& m_font;
    float m_scale;
};

// Clips the list for its lifetime.
class PainterClip
{
public:
    PainterClip(DrawList& list, const Rect& rect)
        : m_list(list)
    {
        m_list.push_clip(rect);
    }
    ~PainterClip() { m_list.pop_clip(); }

    PainterClip(const PainterClip&) = delete;
    PainterClip& operator=(const PainterClip&) = delete;

private:
    DrawList& m_list;
};

} // namespace oryx
