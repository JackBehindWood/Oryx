#pragma once

#include "Oryx/Interface/Canvas/Rect.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Text/TextLayout.h"

namespace oryx
{

inline constexpr uint32_t k_no_clip = 0xFFFFFFFFu;

// Every command carries the index of the clip it was recorded under (DrawList::clip), or k_no_clip.
struct RectCmd
{
    Rect rect;
    Colour colour;
    uint32_t clip = k_no_clip;
};

struct RoundedRectCmd
{
    Rect rect;
    CornerRadius radius;
    Colour colour;
    uint32_t clip = k_no_clip;
};

// The outline sits inside `rect`.
struct BorderCmd
{
    Rect rect;
    CornerRadius radius;
    float thickness = 1.0f;
    Colour colour;
    uint32_t clip = k_no_clip;
};

struct LineCmd
{
    Vec2f from;
    Vec2f to;
    float thickness = 1.0f;
    Colour colour;
    uint32_t clip = k_no_clip;
};

// `origin` is the baseline point: x per `align`, y the baseline. The characters live in the list's text pool.
struct TextCmd
{
    Vec2f origin;
    float pixel_height = 16.0f;
    TextAlign align = TextAlign::Left;
    Colour colour;
    uint32_t first = 0;
    uint32_t length = 0;
    uint32_t clip = k_no_clip;
};

// Which command array a run indexes; a new primitive adds an array and a kind, existing code is untouched.
enum class DrawKind : uint32_t
{
    Rect,
    RoundedRect,
    Border,
    Line,
    Text
};

inline constexpr uint32_t DRAW_KIND_COUNT = 5;

// Consecutive commands of one kind, so the draw order across kinds survives the typed arrays.
struct DrawRun
{
    DrawKind kind = DrawKind::Rect;
    uint32_t first = 0;
    uint32_t count = 0;
};

// One z-order channel.
struct DrawChannel
{
    std::vector<RectCmd> rects;
    std::vector<RoundedRectCmd> rounded_rects;
    std::vector<BorderCmd> borders;
    std::vector<LineCmd> lines;
    std::vector<TextCmd> texts;
    std::vector<DrawRun> runs;
};

// Calls the visitor with each command of the run in order. This is the one place that maps a run's kind to its command array: the visitor needs an operator() per command type, so a new primitive fails to compile wherever it is not handled.
template<typename Visitor>
void for_each_command(const DrawChannel& channel, const DrawRun& run, Visitor&& visitor)
{
    for (uint32_t at = run.first; at < run.first + run.count; ++at)
    {
        switch (run.kind)
        {
            case DrawKind::Rect: visitor(channel.rects[at]); break;
            case DrawKind::RoundedRect: visitor(channel.rounded_rects[at]); break;
            case DrawKind::Border: visitor(channel.borders[at]); break;
            case DrawKind::Line: visitor(channel.lines[at]); break;
            case DrawKind::Text: visitor(channel.texts[at]); break;
        }
    }
}

// Recorded 2D draw commands in logical points (origin top-left, y down), replayed once per frame by `replay`.
// A later channel draws over an earlier one; within a channel, order is recording order. Vectors keep their capacity across `clear`, so a warm list allocates nothing.
// Commands wholly outside the current clip or with no area are not recorded. Main thread only.
class DrawList
{
public:
    DrawList();

    // Throws Error while a clip is open.
    void clear();
    // Like clear but also closes any open clip, for recovering after an error.
    void abandon();

    [[nodiscard]] uint32_t surface() const { return m_surface; }
    void set_surface(uint32_t surface) { m_surface = surface; }

    // Channels 0..count-1 exist and recording goes to channel 0. Throws Error for a zero count.
    void split_channels(uint32_t count);
    // Throws Error when out of range.
    void set_channel(uint32_t index);
    // Appends channels 1.. to channel 0 in order and leaves one channel.
    void merge();
    [[nodiscard]] uint32_t channel_count() const { return m_channel_count; }
    [[nodiscard]] uint32_t current_channel() const { return m_current; }
    [[nodiscard]] const DrawChannel& channel(uint32_t index) const;

    // Pushes the rect intersected with the clip in effect. Throws Error when pop_clip has nothing to pop.
    void push_clip(const Rect& rect);
    void pop_clip();
    [[nodiscard]] uint32_t clip_depth() const { return static_cast<uint32_t>(m_clip_stack.size()); }
    [[nodiscard]] bool has_clip() const { return !m_clip_stack.empty(); }
    // A rect larger than any layout while no clip is open.
    [[nodiscard]] Rect current_clip() const;
    [[nodiscard]] const Rect& clip(uint32_t index) const { return m_clips[index]; }

    void add_rect(const Rect& rect, const Colour& colour);
    void add_rounded_rect(const Rect& rect, const CornerRadius& radius, const Colour& colour);
    void add_border(const Rect& rect, const CornerRadius& radius, float thickness, const Colour& colour);
    void add_line(const Vec2f& from, const Vec2f& to, float thickness, const Colour& colour);
    void add_text(const Vec2f& origin, std::string_view text, float pixel_height, TextAlign align, const Colour& colour);

    [[nodiscard]] std::string_view text(const TextCmd& command) const { return std::string_view(m_text).substr(command.first, command.length); }
    [[nodiscard]] uint32_t command_count() const;

private:
    [[nodiscard]] bool visible(const Rect& rect) const;
    [[nodiscard]] uint32_t clip_index() const { return m_clip_stack.empty() ? k_no_clip : m_clip_stack.back(); }
    void record(DrawKind kind, size_t array_size);

    std::vector<DrawChannel> m_channels;
    std::vector<Rect> m_clips;
    std::vector<uint32_t> m_clip_stack;
    std::string m_text;
    uint32_t m_channel_count = 1;
    uint32_t m_current = 0;
    uint32_t m_surface = 0;
};

// One line per command in draw order with fixed two-decimal numbers, for golden tests.
[[nodiscard]] std::string dump(const DrawList& list);

} // namespace oryx
