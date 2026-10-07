#include "oxpch.h"
#include "Oryx/Interface/Canvas/ImContext.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

void ImContext::require_frame(const char* what) const
{
    if (!m_open)
    {
        throw Error(std::string("ImContext::") + what + " needs an open frame", "call begin_frame first");
    }
}

void ImContext::begin_frame(const ImInput& input)
{
    if (m_open)
    {
        throw Error("ImContext frame is already open", "end_frame, or abort_frame after an error");
    }
    ++m_frame;
    m_open = true;
    m_input = input;
    m_draw.clear();
    m_layout.clear();
    m_draw.set_surface(input.surface);
    m_arena.reset();
    m_id_stack.clear();
    m_frame_items.clear();
    m_hot = {};
    m_items = 0;
}

void ImContext::end_frame()
{
    require_frame("end_frame");
    if (!m_id_stack.empty())
    {
        throw Error("ImContext frame ended with an id scope open", "every push_id needs a pop_id");
    }
    if (m_draw.has_clip())
    {
        throw Error("ImContext frame ended with a clip open", "every push_clip needs a pop_clip");
    }
    if (m_layout.open_depth() != 0)
    {
        throw Error("ImContext frame ended with a box open", "every begin_box needs an end_box");
    }
    solve_layout();
    if (!button_of(m_input, MouseCode::Left).down)
    {
        m_active = {};
    }
    if (button_of(m_input, MouseCode::Left).pressed && !is_valid(m_hot))
    {
        m_focus = {};
    }
    m_memory.collect(m_frame);
    m_open = false;
}

void ImContext::abort_frame()
{
    m_draw.abandon();
    m_layout.clear();
    m_id_stack.clear();
    m_active = {};
    m_open = false;
}

void ImContext::solve_layout()
{
    TextMeasure measure;
    if (m_theme.font != nullptr)
    {
        measure.user = m_theme.font;
        measure.width = [](void* user, std::string_view text, float pixel_height) { return static_cast<Font*>(user)->measure(text, pixel_height).width; };
    }
    m_layout.solve({ { 0.0f, 0.0f }, m_input.surface_size }, measure, m_frame);
    m_layout.paint(m_draw, m_theme.font, m_input.scale);
    for (uint32_t index = 0; index < m_layout.node_count(); ++index)
    {
        const LayoutNode& node = m_layout.node(index);
        ItemMemory* memory = is_valid(node.id) ? m_memory.find_mut(node.id) : nullptr;
        if (memory != nullptr && memory->seen_frame == m_frame)
        {
            memory->rect = node.rect;
        }
    }
}

Painter ImContext::painter(float scale)
{
    if (m_theme.font == nullptr)
    {
        throw Error("ImContext theme has no font", "set ImTheme::font before painting");
    }
    return Painter(m_draw, *m_theme.font, scale);
}

void ImContext::push_id(std::string_view label)
{
    require_frame("push_id");
    m_id_stack.push_back(make_id(label, current_id()));
}

void ImContext::push_id(Id id)
{
    require_frame("push_id");
    m_id_stack.push_back(make_index_id(id.value, current_id()));
}

void ImContext::pop_id()
{
    require_frame("pop_id");
    if (m_id_stack.empty())
    {
        throw Error("ImContext has no id scope to pop", "every pop_id needs a push_id");
    }
    m_id_stack.pop_back();
}

ItemState ImContext::item(Id id, const Rect& rect)
{
    require_frame("item");
    if (!is_valid(id))
    {
        throw Error("ImContext item needs a valid id");
    }
    auto seen = std::lower_bound(m_frame_items.begin(), m_frame_items.end(), id.value);
    if (seen != m_frame_items.end() && *seen == id.value)
    {
        throw Error("ImContext item id used twice in one frame", "push_id a scope or give the widget a distinct label");
    }
    m_frame_items.insert(seen, id.value);
    ++m_items;

    ItemMemory& memory = m_memory.get(id, m_frame);
    memory.has_previous = memory.seen_frame != 0 && memory.seen_frame + 1 == m_frame;
    memory.previous_rect = memory.rect;
    memory.seen_frame = m_frame;
    memory.rect = rect;

    ItemState state;
    const ImButton& left = button_of(m_input, MouseCode::Left);
    const Rect hit = intersect(at_least(rect, Vec2f(m_theme.min_hit_size, m_theme.min_hit_size)), m_draw.current_clip());
    const bool inside = m_input.pointer.valid && contains(hit, m_input.pointer.position);
    state.hovered = inside && (!is_valid(m_active) || m_active == id);
    if (state.hovered)
    {
        m_hot = id;
        if (left.pressed)
        {
            m_active = id;
            state.pressed = true;
        }
    }
    if (m_active == id)
    {
        state.held = left.down;
        if (left.released)
        {
            state.clicked = state.hovered;
            m_active = {};
        }
    }
    return state;
}

ItemState ImContext::item(Id id)
{
    Rect rect;
    layout_rect(id, rect);
    return item(id, rect);
}

uint32_t ImContext::begin_box(std::string_view label, const LayoutStyle& style)
{
    require_frame("begin_box");
    return m_layout.begin_box(id(label), style, m_arena.store(label));
}

uint32_t ImContext::begin_box(Id id, const LayoutStyle& style)
{
    require_frame("begin_box");
    return m_layout.begin_box(id, style);
}

void ImContext::end_box()
{
    require_frame("end_box");
    m_layout.end_box();
}

bool ImContext::previous_rect(Id id, Rect& out) const
{
    const ItemMemory* memory = m_memory.find(id);
    if (memory == nullptr)
    {
        return false;
    }
    if (memory->seen_frame == m_frame)
    {
        out = memory->previous_rect;
        return memory->has_previous;
    }
    out = memory->rect;
    return memory->seen_frame != 0 && memory->seen_frame + 1 == m_frame;
}

ImStats ImContext::stats() const
{
    return { m_frame, m_items, m_draw.command_count(), static_cast<uint32_t>(m_memory.size()), m_arena.used(), m_arena.capacity(), m_layout.node_count() };
}

std::string dump_layout(const ImContext& context)
{
    return dump_layout(context.layout());
}

} // namespace oryx
