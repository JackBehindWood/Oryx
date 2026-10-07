#include "oxpch.h"
#include "Oryx/Interface/Canvas/Replay.h"

namespace oryx
{

namespace
{

class Replayer
{
public:
    Replayer(const DrawList& list, BatchRenderer2D& batcher, Font& font, const ReplayTarget& target)
        : m_list(list)
        , m_batcher(batcher)
        , m_font(font)
        , m_target(target)
    {
    }

    void finish() { set_clip(k_no_clip); }

    void run(const DrawChannel& channel, const DrawRun& run)
    {
        for_each_command(channel, run, [this](const auto& command) { draw(command); });
    }

private:
    [[nodiscard]] Vec2f world(const Vec2f& point) const { return Vec2f(m_target.origin[0] + point[0], m_target.window[1] - (m_target.origin[1] + point[1])); }

    void set_clip(uint32_t clip)
    {
        if (clip == m_clip)
        {
            return;
        }
        if (m_clip != k_no_clip)
        {
            m_batcher.pop_clip();
        }
        m_clip = clip;
        if (clip != k_no_clip)
        {
            const Rect& rect = m_list.clip(clip);
            m_batcher.push_clip(world(rect_centre(rect)), rect.size);
        }
    }

    void draw(const RectCmd& command)
    {
        set_clip(command.clip);
        m_batcher.draw_rect(world(rect_centre(command.rect)), command.rect.size, command.colour);
    }

    void draw(const LineCmd& command)
    {
        set_clip(command.clip);
        const Vec2f a = world(command.from);
        const Vec2f b = world(command.to);
        const Vec2f direction = b - a;
        const float length = std::sqrt(direction[0] * direction[0] + direction[1] * direction[1]);
        if (!(length > 0.0f))
        {
            return;
        }
        const Vec2f normal(-direction[1] / length * command.thickness * 0.5f, direction[0] / length * command.thickness * 0.5f);
        const Vec2f corners[4] = { a - normal, b - normal, b + normal, a + normal };
        m_batcher.draw_quad(corners, command.colour);
    }

    void draw(const TextCmd& command)
    {
        set_clip(command.clip);
        TextStyle style;
        style.pixel_height = command.pixel_height;
        style.colour = command.colour;
        style.align = command.align;
        m_batcher.draw_text(world(command.origin), m_list.text(command), m_font, style);
    }

    // Points around the rounded rect, clockwise from the top-left corner's end of the top edge; `count` per corner.
    static uint32_t outline(const Rect& rect, const CornerRadius& radius, float inset, uint32_t per_corner, Vec2f* points)
    {
        const Vec2f max = rect_max(rect);
        const float radii[4] = { radius.top_left, radius.top_right, radius.bottom_right, radius.bottom_left };
        const Vec2f centres[4] = {
            Vec2f(rect.min[0] + radii[0], rect.min[1] + radii[0]),
            Vec2f(max[0] - radii[1], rect.min[1] + radii[1]),
            Vec2f(max[0] - radii[2], max[1] - radii[2]),
            Vec2f(rect.min[0] + radii[3], max[1] - radii[3]),
        };
        const Vec2f corners[4] = { rect.min, Vec2f(max[0], rect.min[1]), max, Vec2f(rect.min[0], max[1]) };
        const Vec2f inward[4] = { Vec2f(1.0f, 1.0f), Vec2f(-1.0f, 1.0f), Vec2f(-1.0f, -1.0f), Vec2f(1.0f, -1.0f) };
        uint32_t written = 0;
        for (uint32_t corner = 0; corner < 4; ++corner)
        {
            const float start = -math::PI<float> + math::PI<float> * 0.5f * static_cast<float>(corner);
            const float r = math::max(radii[corner] - inset, 0.0f);
            for (uint32_t step = 0; step < per_corner; ++step)
            {
                if (radii[corner] <= 0.0f || per_corner == 1)
                {
                    points[written++] = Vec2f(corners[corner][0] + inward[corner][0] * inset, corners[corner][1] + inward[corner][1] * inset);
                    continue;
                }
                const float angle = start + math::PI<float> * 0.5f * static_cast<float>(step) / static_cast<float>(per_corner - 1);
                points[written++] = Vec2f(centres[corner][0] + std::cos(angle) * r, centres[corner][1] + std::sin(angle) * r);
            }
        }
        return written;
    }

    static uint32_t points_per_corner(const CornerRadius& radius)
    {
        if (is_square(radius))
        {
            return 1;
        }
        const float largest = math::max(math::max(radius.top_left, radius.top_right), math::max(radius.bottom_right, radius.bottom_left));
        return 2 + math::min(static_cast<uint32_t>(largest * 0.5f), 10u);
    }

    void draw(const RoundedRectCmd& command)
    {
        set_clip(command.clip);
        if (is_square(command.radius))
        {
            m_batcher.draw_rect(world(rect_centre(command.rect)), command.rect.size, command.colour);
            return;
        }
        Vec2f points[4 * k_max_corner_points];
        const uint32_t count = outline(command.rect, command.radius, 0.0f, points_per_corner(command.radius), points);
        const Vec2f centre = world(rect_centre(command.rect));
        for (uint32_t i = 0; i < count; ++i)
        {
            m_batcher.draw_triangle(centre, world(points[i]), world(points[(i + 1) % count]), command.colour);
        }
    }

    void draw(const ImageCmd& command)
    {
        if (command.image.index >= m_target.image_count || m_target.images[command.image.index] == nullptr)
        {
            return;
        }
        set_clip(command.clip);
        const Texture2D& texture = *m_target.images[command.image.index];
        const Vec2f centre = world(rect_centre(command.rect));
        if (is_square(command.radius))
        {
            m_batcher.draw_sprite(centre, command.rect.size, texture, command.tint, 0.0f, command.uv_min, command.uv_max);
            return;
        }
        Vec2f points[4 * k_max_corner_points];
        const uint32_t count = outline(command.rect, command.radius, 0.0f, points_per_corner(command.radius), points);
        for (uint32_t i = 0; i < count; ++i)
        {
            points[i] = world(points[i]);
        }
        m_batcher.draw_sprite_polygon(centre, command.rect.size, points, count, texture, command.tint, command.uv_min, command.uv_max);
    }

    void draw(const BorderCmd& command)
    {
        set_clip(command.clip);
        const float thickness = math::min(command.thickness, math::min(command.rect.size[0], command.rect.size[1]) * 0.5f);
        const uint32_t per_corner = points_per_corner(command.radius);
        Vec2f outer[4 * k_max_corner_points];
        Vec2f inner[4 * k_max_corner_points];
        const uint32_t count = outline(command.rect, command.radius, 0.0f, per_corner, outer);
        outline(command.rect, command.radius, thickness, per_corner, inner);
        for (uint32_t i = 0; i < count; ++i)
        {
            const uint32_t next = (i + 1) % count;
            m_batcher.draw_triangle(world(outer[i]), world(outer[next]), world(inner[i]), command.colour);
            m_batcher.draw_triangle(world(outer[next]), world(inner[next]), world(inner[i]), command.colour);
        }
    }

    static constexpr uint32_t k_max_corner_points = 12;

    const DrawList& m_list;
    BatchRenderer2D& m_batcher;
    Font& m_font;
    ReplayTarget m_target;
    uint32_t m_clip = k_no_clip;
};

} // namespace

void replay(const DrawList& list, BatchRenderer2D& batcher, Font& font, const ReplayTarget& target)
{
    OX_PROFILE_SCOPE("replay");
    Replayer replayer(list, batcher, font, target);
    for (uint32_t index = 0; index < list.channel_count(); ++index)
    {
        const DrawChannel& channel = list.channel(index);
        for (const DrawRun& run : channel.runs)
        {
            replayer.run(channel, run);
        }
    }
    replayer.finish();
}

void replay(const DrawList& list, BatchRenderer2D& batcher, Font& font, ReplayTarget target, const Texture2D& image)
{
    const Texture2D* const images[1] = { &image };
    target.images = images;
    target.image_count = 1;
    replay(list, batcher, font, target);
}

} // namespace oryx
