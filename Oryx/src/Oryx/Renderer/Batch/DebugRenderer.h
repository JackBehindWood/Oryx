#pragma once

#include "Oryx/Renderer/Batch/BatchRenderer2D.h"

namespace oryx
{

enum class DebugShape : uint8_t
{
    Line,
    Rect,
    Circle,
    Text
};

// `scalar` is the rotation of a rect or the radius of a circle; `frames` counts the replays left.
struct DebugCommand
{
    DebugShape shape = DebugShape::Line;
    Vec2f a = { 0.0f, 0.0f };
    Vec2f b = { 0.0f, 0.0f };
    Colour colour = { 1.0f, 1.0f, 1.0f, 1.0f };
    float scalar = 0.0f;
    uint32_t frames = 1;
    TextStyle style;
    std::string text;
};

inline constexpr size_t DEBUG_MAX_COMMANDS = 1u << 20;

// Records shapes from anywhere (simulation, strategies, observers) with no scene, camera or device involved, and replays them into a batcher where the app asks.
// A shape lives for `frames` replay frames (default: one). Shapes keep recording order and are drawn without depth. Main thread only.
class DebugRenderer
{
public:
    void line(const Vec2f& a, const Vec2f& b, const Colour& colour, uint32_t frames = 1);
    void rect(const Vec2f& position, const Vec2f& size, const Colour& colour, float rotation = 0.0f, uint32_t frames = 1);
    void circle(const Vec2f& centre, float radius, const Colour& colour, uint32_t frames = 1);
    // Dropped while no font is set; the font must outlive every recorded text.
    void text(const Vec2f& position, std::string_view text, const TextStyle& style = {}, uint32_t frames = 1);

    // Disabled, every call returns before recording; commands already recorded are kept.
    void set_enabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool enabled() const { return m_enabled; }
    void set_font(Font* font) { m_font = font; }
    [[nodiscard]] Font* font() const { return m_font; }

    // Draws every recorded shape through the batcher, which must have an open scene.
    void render(BatchRenderer2D& batcher) const;
    // Ages the shapes by one frame and drops the expired ones.
    void end_frame();
    void clear();

    [[nodiscard]] size_t size() const { return m_commands.size(); }
    // Shapes refused because DEBUG_MAX_COMMANDS was reached.
    [[nodiscard]] uint32_t dropped() const { return m_dropped; }

private:
    DebugCommand* add(DebugShape shape, uint32_t frames);

    std::vector<DebugCommand> m_commands;
    Font* m_font = nullptr;
    uint32_t m_dropped = 0;
    bool m_enabled = true;
};

} // namespace oryx
