#include "oxpch.h"
#include "Oryx/Interface/Canvas/Painter.h"

#include "Oryx/Core/Utf8.h"

namespace oryx
{

namespace
{

constexpr std::string_view k_ellipsis = "...";
// A stack buffer, because a Painter is rebuilt every frame and a string member would allocate again each time.
constexpr size_t k_truncation_capacity = 256;

}

void Painter::fill_rect(const Rect& rect, const Colour& colour)
{
    m_list.add_rect(snap_to_pixels(rect, m_scale), colour);
}

void Painter::fill_rounded_rect(const Rect& rect, const CornerRadius& radius, const Colour& colour)
{
    m_list.add_rounded_rect(snap_to_pixels(rect, m_scale), radius, colour);
}

void Painter::border(const Rect& rect, const CornerRadius& radius, float thickness, const Colour& colour)
{
    m_list.add_border(snap_to_pixels(rect, m_scale), radius, thickness, colour);
}

void Painter::image(const Rect& rect, ImageHandle image, const ImageSpec& spec)
{
    m_list.add_image(snap_to_pixels(rect, m_scale), image, spec.uv_min, spec.uv_max, spec.radius, spec.tint);
}

void Painter::line(const Vec2f& from, const Vec2f& to, const Colour& colour, float thickness)
{
    m_list.add_line(from, to, thickness, colour);
}

void Painter::icon(const Rect& box, Icon icon, const Colour& colour, float size, float thickness)
{
    draw_icon(m_list, box, icon, colour, size, thickness);
}

float Painter::text_width(std::string_view text, float pixel_height)
{
    return m_font.measure(text, pixel_height).width;
}

void Painter::text(const Rect& box, std::string_view text, const TextSpec& spec)
{
    if (text.empty() || !m_font.ready() || is_empty(box))
    {
        return;
    }
    bool clip_to_box = false;
    char truncated[k_truncation_capacity];
    if (text_width(text, spec.pixel_height) > box.size[0])
    {
        if (spec.ellipsis)
        {
            const float available = box.size[0] - text_width(k_ellipsis, spec.pixel_height);
            size_t keep = 0;
            size_t index = 0;
            while (index < text.size())
            {
                decode_utf8(text, index);
                if (text_width(text.substr(0, index), spec.pixel_height) > available)
                {
                    break;
                }
                keep = index;
            }
            keep = math::min(keep, k_truncation_capacity - k_ellipsis.size());
            std::memcpy(truncated, text.data(), keep);
            std::memcpy(truncated + keep, k_ellipsis.data(), k_ellipsis.size());
            text = std::string_view(truncated, keep + k_ellipsis.size());
        }
        else
        {
            clip_to_box = true;
        }
    }
    float x = box.min[0];
    if (spec.align == TextAlign::Centre)
    {
        x += box.size[0] * 0.5f;
    }
    else if (spec.align == TextAlign::Right)
    {
        x += box.size[0];
    }
    const float baseline = snap_to_pixel(rect_centre(box)[1] + m_font.ascent(spec.pixel_height) * 0.5f, m_scale);
    if (clip_to_box)
    {
        m_list.push_clip(box);
    }
    m_list.add_text(Vec2f(snap_to_pixel(x, m_scale), baseline), text, spec.pixel_height, spec.align, spec.colour);
    if (clip_to_box)
    {
        m_list.pop_clip();
    }
}

} // namespace oryx
