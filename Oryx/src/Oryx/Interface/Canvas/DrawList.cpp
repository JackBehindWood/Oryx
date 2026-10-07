#include "oxpch.h"
#include "Oryx/Interface/Canvas/DrawList.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

namespace
{

template<typename T>
void append(std::vector<T>& destination, std::vector<T>& source)
{
    destination.insert(destination.end(), source.begin(), source.end());
    source.clear();
}

} // namespace

DrawList::DrawList()
    : m_channels(1)
{
}

void DrawList::clear()
{
    if (!m_clip_stack.empty())
    {
        throw Error("DrawList cleared with a clip open", "every push_clip needs a pop_clip");
    }
    for (DrawChannel& channel : m_channels)
    {
        channel.rects.clear();
        channel.rounded_rects.clear();
        channel.borders.clear();
        channel.lines.clear();
        channel.texts.clear();
        channel.images.clear();
        channel.runs.clear();
    }
    m_clips.clear();
    m_text.clear();
    m_channel_count = 1;
    m_current = 0;
}

void DrawList::abandon()
{
    m_clip_stack.clear();
    clear();
}

void DrawList::split_channels(uint32_t count)
{
    if (count == 0)
    {
        throw Error("DrawList needs at least one channel");
    }
    if (m_channels.size() < count)
    {
        m_channels.resize(count);
    }
    m_channel_count = count;
    m_current = 0;
}

void DrawList::set_channel(uint32_t index)
{
    if (index >= m_channel_count)
    {
        throw Error("DrawList channel out of range", "split_channels(count) first");
    }
    m_current = index;
}

void DrawList::merge()
{
    DrawChannel& target = m_channels[0];
    for (uint32_t index = 1; index < m_channel_count; ++index)
    {
        DrawChannel& source = m_channels[index];
        const uint32_t offsets[DRAW_KIND_COUNT] = {
            static_cast<uint32_t>(target.rects.size()),
            static_cast<uint32_t>(target.rounded_rects.size()),
            static_cast<uint32_t>(target.borders.size()),
            static_cast<uint32_t>(target.lines.size()),
            static_cast<uint32_t>(target.texts.size()),
            static_cast<uint32_t>(target.images.size()),
        };
        for (DrawRun run : source.runs)
        {
            run.first += offsets[static_cast<uint32_t>(run.kind)];
            target.runs.push_back(run);
        }
        append(target.rects, source.rects);
        append(target.rounded_rects, source.rounded_rects);
        append(target.borders, source.borders);
        append(target.lines, source.lines);
        append(target.texts, source.texts);
        append(target.images, source.images);
        source.runs.clear();
    }
    m_channel_count = 1;
    m_current = 0;
}

const DrawChannel& DrawList::channel(uint32_t index) const
{
    if (index >= m_channel_count)
    {
        throw Error("DrawList channel out of range");
    }
    return m_channels[index];
}

void DrawList::push_clip(const Rect& rect)
{
    const Rect clipped = m_clip_stack.empty() ? rect : intersect(rect, m_clips[m_clip_stack.back()]);
    if (!m_clip_stack.empty() && m_clips[m_clip_stack.back()] == clipped)
    {
        m_clip_stack.push_back(m_clip_stack.back());
        return;
    }
    m_clips.push_back(clipped);
    m_clip_stack.push_back(static_cast<uint32_t>(m_clips.size() - 1));
}

void DrawList::pop_clip()
{
    if (m_clip_stack.empty())
    {
        throw Error("DrawList has no clip to pop", "every pop_clip needs a push_clip");
    }
    m_clip_stack.pop_back();
}

Rect DrawList::current_clip() const
{
    if (m_clip_stack.empty())
    {
        return unbounded_rect();
    }
    return m_clips[m_clip_stack.back()];
}

bool DrawList::visible(const Rect& rect) const
{
    return !is_empty(rect) && (m_clip_stack.empty() || overlaps(rect, m_clips[m_clip_stack.back()]));
}

void DrawList::record(DrawKind kind, size_t array_size)
{
    std::vector<DrawRun>& runs = m_channels[m_current].runs;
    if (!runs.empty() && runs.back().kind == kind)
    {
        ++runs.back().count;
        return;
    }
    runs.push_back({ kind, static_cast<uint32_t>(array_size - 1), 1 });
}

void DrawList::add_rect(const Rect& rect, const Colour& colour)
{
    if (!visible(rect))
    {
        return;
    }
    std::vector<RectCmd>& array = m_channels[m_current].rects;
    array.push_back({ rect, colour, clip_index() });
    record(DrawKind::Rect, array.size());
}

void DrawList::add_rounded_rect(const Rect& rect, const CornerRadius& radius, const Colour& colour)
{
    if (!visible(rect))
    {
        return;
    }
    std::vector<RoundedRectCmd>& array = m_channels[m_current].rounded_rects;
    array.push_back({ rect, clamp_radius(radius, rect.size), colour, clip_index() });
    record(DrawKind::RoundedRect, array.size());
}

void DrawList::add_border(const Rect& rect, const CornerRadius& radius, float thickness, const Colour& colour)
{
    if (!visible(rect) || !(thickness > 0.0f))
    {
        return;
    }
    std::vector<BorderCmd>& array = m_channels[m_current].borders;
    array.push_back({ rect, clamp_radius(radius, rect.size), thickness, colour, clip_index() });
    record(DrawKind::Border, array.size());
}

void DrawList::add_line(const Vec2f& from, const Vec2f& to, float thickness, const Colour& colour)
{
    const Vec2f low(math::min(from[0], to[0]), math::min(from[1], to[1]));
    const Vec2f high(math::max(from[0], to[0]), math::max(from[1], to[1]));
    const float reach = thickness * 0.5f;
    if (!(thickness > 0.0f) || !visible({ Vec2f(low[0] - reach, low[1] - reach), Vec2f(high[0] - low[0] + thickness, high[1] - low[1] + thickness) }))
    {
        return;
    }
    std::vector<LineCmd>& array = m_channels[m_current].lines;
    array.push_back({ from, to, thickness, colour, clip_index() });
    record(DrawKind::Line, array.size());
}

void DrawList::add_image(const Rect& rect, ImageHandle image, const Vec2f& uv_min, const Vec2f& uv_max, const CornerRadius& radius, const Colour& tint)
{
    if (!visible(rect))
    {
        return;
    }
    std::vector<ImageCmd>& array = m_channels[m_current].images;
    array.push_back({ rect, image, uv_min, uv_max, clamp_radius(radius, rect.size), tint, clip_index() });
    record(DrawKind::Image, array.size());
}

void DrawList::add_text(const Vec2f& origin, std::string_view text, float pixel_height, TextAlign align, const Colour& colour)
{
    if (text.empty() || (!m_clip_stack.empty() && is_empty(m_clips[m_clip_stack.back()])))
    {
        return;
    }
    std::vector<TextCmd>& array = m_channels[m_current].texts;
    array.push_back({ origin, pixel_height, align, colour, static_cast<uint32_t>(m_text.size()), static_cast<uint32_t>(text.size()), clip_index() });
    m_text.append(text);
    record(DrawKind::Text, array.size());
}

uint32_t DrawList::command_count() const
{
    uint32_t total = 0;
    for (uint32_t index = 0; index < m_channel_count; ++index)
    {
        const DrawChannel& channel = m_channels[index];
        total += static_cast<uint32_t>(channel.rects.size() + channel.rounded_rects.size() + channel.borders.size() + channel.lines.size() + channel.texts.size() + channel.images.size());
    }
    return total;
}

uint32_t DrawList::command_count(DrawKind kind) const
{
    size_t total = 0;
    for (uint32_t index = 0; index < m_channel_count; ++index)
    {
        const DrawChannel& channel = m_channels[index];
        switch (kind)
        {
            case DrawKind::Rect: total += channel.rects.size(); break;
            case DrawKind::RoundedRect: total += channel.rounded_rects.size(); break;
            case DrawKind::Border: total += channel.borders.size(); break;
            case DrawKind::Line: total += channel.lines.size(); break;
            case DrawKind::Text: total += channel.texts.size(); break;
            case DrawKind::Image: total += channel.images.size(); break;
        }
    }
    return static_cast<uint32_t>(total);
}

} // namespace oryx
