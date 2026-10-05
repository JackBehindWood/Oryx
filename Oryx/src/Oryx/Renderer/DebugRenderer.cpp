#include "oxpch.h"
#include "Oryx/Renderer/DebugRenderer.h"

namespace oryx
{

DebugCommand* DebugRenderer::add(DebugShape shape, uint32_t frames)
{
    if (!m_enabled)
    {
        return nullptr;
    }
    if (m_commands.size() >= DEBUG_MAX_COMMANDS)
    {
        ++m_dropped;
        return nullptr;
    }
    DebugCommand& command = m_commands.emplace_back();
    command.shape = shape;
    command.frames = std::max(frames, 1u);
    return &command;
}

void DebugRenderer::line(const Vec2f& a, const Vec2f& b, const Colour& colour, uint32_t frames)
{
    if (DebugCommand* command = add(DebugShape::Line, frames))
    {
        command->a = a;
        command->b = b;
        command->colour = colour;
    }
}

void DebugRenderer::rect(const Vec2f& position, const Vec2f& size, const Colour& colour, float rotation, uint32_t frames)
{
    if (DebugCommand* command = add(DebugShape::Rect, frames))
    {
        command->a = position;
        command->b = size;
        command->colour = colour;
        command->scalar = rotation;
    }
}

void DebugRenderer::circle(const Vec2f& centre, float radius, const Colour& colour, uint32_t frames)
{
    if (DebugCommand* command = add(DebugShape::Circle, frames))
    {
        command->a = centre;
        command->colour = colour;
        command->scalar = radius;
    }
}

void DebugRenderer::text(const Vec2f& position, std::string_view text, const TextStyle& style, uint32_t frames)
{
    if (m_font == nullptr)
    {
        return;
    }
    if (DebugCommand* command = add(DebugShape::Text, frames))
    {
        command->a = position;
        command->style = style;
        command->text = text;
    }
}

void DebugRenderer::render(BatchRenderer2D& batcher) const
{
    for (const DebugCommand& command : m_commands)
    {
        switch (command.shape)
        {
        case DebugShape::Line: batcher.draw_line(command.a, command.b, command.colour); break;
        case DebugShape::Rect: batcher.draw_rect(command.a, command.b, command.colour, command.scalar); break;
        case DebugShape::Circle: batcher.draw_circle(command.a, command.scalar, command.colour); break;
        case DebugShape::Text:
            if (m_font != nullptr)
            {
                batcher.draw_text(command.a, command.text, *m_font, command.style);
            }
            break;
        }
    }
}

void DebugRenderer::end_frame()
{
    for (DebugCommand& command : m_commands)
    {
        --command.frames;
    }
    m_commands.erase(std::remove_if(m_commands.begin(), m_commands.end(), [](const DebugCommand& command) { return command.frames == 0; }), m_commands.end());
}

void DebugRenderer::clear()
{
    m_commands.clear();
}

} // namespace oryx
