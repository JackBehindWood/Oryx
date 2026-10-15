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
    OX_PROFILE_SCOPE("ImContext::begin_frame");
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
    m_hits.clear();
    m_hot = {};
    m_items = 0;
    m_wheel_consumed = false;
    m_time += input.delta_time;
    m_output = {};
    m_press_claimed = false;
    m_popup_count = 0;
    m_popup_depth = 0;
    m_shield_count = 0;
    m_shield_depth = 0;
    m_disabled = 0;
    m_scope_depth = 0;
    m_layout.set_alpha(1.0f);
    m_animating = m_animating_next;
    m_animating_next = false;
    for (AnimSlot& slot : m_anim)
    {
        if (is_valid(slot.id) && slot.touched + 1 < m_frame)
        {
            slot.id = ImId{};
        }
    }
}

uint32_t ImContext::animation_count() const
{
    uint32_t count = 0;
    for (const AnimSlot& slot : m_anim)
    {
        count += is_valid(slot.id) ? 1u : 0u;
    }
    return count;
}

float* ImContext::animate_values(ImId id, const float* target, const float* rest, uint32_t count, const AnimSpec& spec)
{
    static thread_local float result[4];
    const auto same = [count](const float* a, const float* b) {
        for (uint32_t i = 0; i < count; ++i)
        {
            if (a[i] != b[i])
            {
                return false;
            }
        }
        return true;
    };
    const auto give = [&](const float* values) {
        for (uint32_t i = 0; i < count; ++i)
        {
            result[i] = values[i];
        }
        return result;
    };
    if (!m_animations || !is_valid(id))
    {
        return give(target);
    }
    AnimSlot* slot = nullptr;
    AnimSlot* free_slot = nullptr;
    for (AnimSlot& candidate : m_anim)
    {
        if (candidate.id == id)
        {
            slot = &candidate;
            break;
        }
        free_slot = !is_valid(candidate.id) && free_slot == nullptr ? &candidate : free_slot;
    }
    if (slot == nullptr)
    {
        if (same(target, rest) || free_slot == nullptr)
        {
            return give(target);
        }
        slot = free_slot;
        *slot = AnimSlot{};
        slot->id = id;
        for (uint32_t i = 0; i < count; ++i)
        {
            slot->from[i] = rest[i];
            slot->value[i] = rest[i];
            slot->to[i] = target[i];
        }
    }
    else if (!same(target, slot->to))
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            slot->from[i] = slot->value[i];
            slot->to[i] = target[i];
        }
        slot->t = 0.0f;
        slot->advanced = 0;
    }
    slot->touched = m_frame;
    if (slot->advanced != m_frame)
    {
        slot->advanced = m_frame;
        slot->t = math::min(slot->t + m_input.delta_time * m_animation_speed / math::max(spec.seconds, 0.001f), 1.0f);
        const float eased = ease(spec.easing, slot->t);
        for (uint32_t i = 0; i < count; ++i)
        {
            slot->value[i] = slot->t >= 1.0f ? slot->to[i] : math::lerp(slot->from[i], slot->to[i], eased);
        }
    }
    if (slot->t < 1.0f)
    {
        m_animating_next = true;
    }
    else if (same(slot->to, rest))
    {
        slot->id = ImId{};
        return give(rest);
    }
    return give(slot->value);
}

float ImContext::animate(ImId id, float target, float rest, const AnimSpec& spec)
{
    return animate_values(id, &target, &rest, 1, spec)[0];
}

Vec2f ImContext::animate(ImId id, const Vec2f& target, const Vec2f& rest, const AnimSpec& spec)
{
    const float to[2] = { target[0], target[1] };
    const float from[2] = { rest[0], rest[1] };
    const float* value = animate_values(id, to, from, 2, spec);
    return { value[0], value[1] };
}

Colour ImContext::animate(ImId id, const Colour& target, const Colour& rest, const AnimSpec& spec)
{
    const float to[4] = { target.r, target.g, target.b, target.a };
    const float from[4] = { rest.r, rest.g, rest.b, rest.a };
    const float* value = animate_values(id, to, from, 4, spec);
    return { value[0], value[1], value[2], value[3] };
}

Vec2f ImContext::consume_wheel()
{
    require_frame("consume_wheel");
    if (m_wheel_consumed)
    {
        return { 0.0f, 0.0f };
    }
    m_wheel_consumed = true;
    return m_input.wheel;
}

void ImContext::end_frame()
{
    OX_PROFILE_SCOPE("ImContext::end_frame");
    require_frame("end_frame");
    if (!m_id_stack.empty())
    {
        throw Error("ImContext frame ended with an id scope open", "every push_id needs a pop_id");
    }
    if (m_scope_depth != 0)
    {
        throw Error(std::string("ImContext frame ended with ") + m_scopes[m_scope_depth - 1].name + " open", "every begin_* of a scoped widget needs its end_*");
    }
    if (m_draw.has_clip())
    {
        throw Error("ImContext frame ended with a clip open", "every push_clip needs a pop_clip");
    }
    if (m_layout.open_depth() != 0)
    {
        throw Error("ImContext frame ended with a box open", "every begin_box needs an end_box");
    }
    if (m_disabled != 0)
    {
        throw Error("ImContext frame ended with a disabled scope open", "every begin_disabled needs an end_disabled");
    }
    if (m_shield_depth != 0)
    {
        throw Error("ImContext frame ended with a shield layer open", "every begin_shield_layer needs an end_shield_layer");
    }
    if (m_popup_depth != 0)
    {
        throw Error("ImContext frame ended with a popup layer open", "every begin_popup_layer needs an end_popup_layer");
    }
    solve_layout();
    m_arena_high_water = math::max(m_arena_high_water, m_arena.used());
    if (!button_of(m_input, MouseCode::Left).down)
    {
        m_active = {};
    }
    if (!button_of(m_input, MouseCode::Left).down)
    {
        m_drag_id = {};
        m_dragging = false;
    }
    if (button_of(m_input, MouseCode::Left).pressed && !is_valid(m_hot))
    {
        m_focus = {};
    }
    m_memory.collect(m_frame);
    m_state.collect(m_frame);
    m_last_hits.swap(m_hits);
    m_open = false;
}

void ImContext::abort_frame()
{
    m_draw.abandon();
    m_layout.clear();
    m_id_stack.clear();
    m_active = {};
    m_hits.clear();
    m_popup_depth = 0;
    m_popup_count = 0;
    m_shield_depth = 0;
    m_shield_count = 0;
    m_disabled = 0;
    m_scope_depth = 0;
    m_layout.set_alpha(1.0f);
    m_open = false;
}

void ImContext::scope_overflow(const char* name) const
{
    throw Error(std::string("Too many nested scopes opening ") + name, "at most 24 scoped widgets may be open at once");
}

void ImContext::scope_missing(const char* user) const
{
    throw Error(std::string(user) + " called outside its widget", "open it with its begin_* first");
}

void ImContext::scope_mismatch(const char* end) const
{
    if (m_scope_depth == 0)
    {
        throw Error(std::string(end) + " called with no scoped widget open", "every end_* needs its begin_*, and only when begin_* returned true");
    }
    throw Error(std::string(end) + " called while " + m_scopes[m_scope_depth - 1].name + " is the innermost open scope", "close inner scoped widgets first");
}

void ImContext::solve_layout()
{
    OX_PROFILE_SCOPE("ImContext::solve_layout");
    TextMeasure measure;
    if (m_theme.font != nullptr)
    {
        measure.user = m_theme.font;
        measure.width = [](void* user, std::string_view text, float pixel_height) { return static_cast<Font*>(user)->measure(text, pixel_height).width; };
        measure.tabular_width = [](void* user, std::string_view text, float pixel_height) { return static_cast<Font*>(user)->measure(text, pixel_height, 1.0f, true).width; };
    }
    m_layout.solve({ { 0.0f, 0.0f }, m_input.surface_size }, measure, m_frame);
    m_layout.paint(m_draw, m_theme.font, m_input.scale);
    if (m_draw.channel_count() > 1)
    {
        m_draw.merge();
    }
    for (uint32_t index = 0; index < m_layout.node_count(); ++index)
    {
        const LayoutNode& node = m_layout.node(index);
        ItemMemory* memory = is_valid(node.id) ? m_memory.find_mut(node.id) : nullptr;
        if (memory != nullptr && memory->seen_frame == m_frame)
        {
            memory->rect = node.rect;
        }
    }
    for (uint32_t index = 0; index < m_popup_count; ++index)
    {
        std::ignore = m_layout.rect_of(m_popups[index].id, m_popups[index].rect);
    }
    m_last_shields = m_shields;
    m_last_shield_count = m_shield_count;
    m_last_popups = m_popups;
    m_last_popup_count = m_popup_count;
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
    m_id_stack.push_back(make_im_id(label, hash_parent()));
}

void ImContext::push_id(ImId id)
{
    require_frame("push_id");
    m_id_stack.push_back(make_im_index_id(id.value, hash_parent()));
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

void ImContext::begin_disabled()
{
    require_frame("begin_disabled");
    ++m_disabled;
    m_layout.set_alpha(k_disabled_alpha);
}

void ImContext::end_disabled()
{
    require_frame("end_disabled");
    if (m_disabled == 0)
    {
        throw Error("ImContext has no disabled scope to end", "every end_disabled needs a begin_disabled");
    }
    if (--m_disabled == 0)
    {
        m_layout.set_alpha(1.0f);
    }
}

ItemState ImContext::item(ImId id, const Rect& rect)
{
    return item_clipped(id, rect, unbounded_rect());
}

ItemState ImContext::item_clipped(ImId id, const Rect& rect, const Rect& layout_clip)
{
    require_frame("item");
    if (!is_valid(id))
    {
        throw Error("ImContext item needs a valid id");
    }
    ItemMemory& memory = m_memory.get(id, m_frame);
    if (memory.seen_frame == m_frame)
    {
        throw Error("ImContext item id used twice in one frame", "push_id a scope or give the widget a distinct label");
    }
    ++m_items;

    memory.has_previous = memory.seen_frame != 0 && memory.seen_frame + 1 == m_frame;
    memory.previous_rect = memory.rect;
    memory.seen_frame = m_frame;
    memory.rect = rect;

    ItemState state;
    if (m_disabled != 0)
    {
        return state;
    }
    const ImButton& left = button_of(m_input, MouseCode::Left);
    const Rect hit = intersect(intersect(at_least(rect, Vec2f(m_theme.min_hit_size, m_theme.min_hit_size)), m_draw.current_clip()), layout_clip);
    m_hits.push_back({ id, hit });
    const bool inside = m_input.pointer.valid && contains(hit, m_input.pointer.position);
    const bool covered = inside && ((m_popup_depth == 0 && under_popup(m_input.pointer.position)) || (m_shield_depth == 0 && under_shield(m_input.pointer.position)));
    state.hovered = inside && !covered && (!is_valid(m_active) || m_active == id || (m_press_claimed && left.pressed));
    if (state.hovered)
    {
        memory.hovered_seconds = memory.hovered_frame != 0 && memory.hovered_frame + 1 == m_frame ? memory.hovered_seconds + m_input.delta_time : 0.0f;
        memory.hovered_frame = m_frame;
        state.hovered_seconds = memory.hovered_seconds;
        m_hot = id;
        if (left.pressed)
        {
            m_active = id;
            m_press_claimed = true;
            state.pressed = true;
            m_drag_id = id;
            m_drag_press = m_input.pointer.position;
            m_drag_last = m_drag_press;
            m_dragging = false;
        }
        if (button_of(m_input, MouseCode::Right).released)
        {
            state.right_clicked = true;
        }
    }
    if (m_active == id)
    {
        state.held = left.down;
        if (left.released)
        {
            state.clicked = state.hovered;
            m_active = {};
            if (state.clicked)
            {
                const bool continues = m_last_click_id == id && m_time - m_last_click_time <= m_theme.double_click_seconds;
                m_click_run = continues ? m_click_run + 1 : 1;
                state.double_clicked = m_click_run == 2;
                state.triple_clicked = m_click_run == 3;
                m_last_click_id = m_click_run >= 3 ? ImId{} : id;
                m_last_click_time = m_time;
            }
        }
    }
    return state;
}

bool ImContext::under_popup(const Vec2f& point) const
{
    for (uint32_t index = 0; index < m_last_popup_count; ++index)
    {
        if (contains(m_last_popups[index].rect, point))
        {
            return true;
        }
    }
    return false;
}

bool ImContext::under_shield(const Vec2f& point) const
{
    for (uint32_t index = 0; index < m_last_shield_count; ++index)
    {
        if (contains(m_last_shields[index], point))
        {
            return true;
        }
    }
    return false;
}

void ImContext::add_shield(const Rect& rect)
{
    require_frame("add_shield");
    if (m_shield_count < k_max_shields_per_frame)
    {
        m_shields[m_shield_count++] = rect;
    }
}

void ImContext::begin_shield_layer()
{
    require_frame("begin_shield_layer");
    ++m_shield_depth;
}

void ImContext::end_shield_layer()
{
    require_frame("end_shield_layer");
    if (m_shield_depth == 0)
    {
        throw Error("ImContext has no shield layer to end", "every end_shield_layer needs a begin_shield_layer");
    }
    --m_shield_depth;
}

PopupResult ImContext::begin_popup_layer(ImId box_id)
{
    require_frame("begin_popup_layer");
    if (m_popup_depth >= k_max_popup_depth || m_popup_count >= k_max_popups_per_frame)
    {
        throw Error("ImContext popup layers nest too deep or there are too many in one frame", "close popups that are not needed");
    }
    PopupResult result;
    uint32_t at = k_max_popups_per_frame;
    uint32_t deepest = 0;
    for (uint32_t index = 0; index < m_last_popup_count; ++index)
    {
        at = m_last_popups[index].id == box_id ? index : at;
        deepest = m_last_popups[index].depth > m_last_popups[deepest].depth ? index : deepest;
    }
    if (at < m_last_popup_count)
    {
        const PopupEntry& entry = m_last_popups[at];
        bool inside = false;
        for (uint32_t index = at; index < m_last_popup_count && (index == at || m_last_popups[index].depth > entry.depth); ++index)
        {
            inside = inside || (m_input.pointer.valid && contains(m_last_popups[index].rect, m_input.pointer.position));
        }
        result.closed_by_outside = button_of(m_input, MouseCode::Left).pressed && m_input.pointer.valid && !inside;
        result.closed_by_escape = key_pressed(m_input.keys, ImKey::Escape) && at == deepest;
    }
    m_popups[m_popup_count++] = { box_id, {}, m_popup_depth };
    ++m_popup_depth;
    return result;
}

ImId ImContext::popup_id() const
{
    for (uint32_t index = m_popup_count; index-- > 0 && m_popup_depth != 0;)
    {
        if (m_popups[index].depth + 1 == m_popup_depth)
        {
            return m_popups[index].id;
        }
    }
    return {};
}

void ImContext::end_popup_layer()
{
    require_frame("end_popup_layer");
    if (m_popup_depth == 0)
    {
        throw Error("ImContext has no popup layer to end", "every end_popup_layer needs a begin_popup_layer");
    }
    --m_popup_depth;
}

ItemDrag ImContext::item_drag(ImId id)
{
    require_frame("item_drag");
    ItemDrag drag;
    if (m_drag_id != id || !is_valid(id))
    {
        return drag;
    }
    const ImButton& left = button_of(m_input, MouseCode::Left);
    const Vec2f position = m_input.pointer.position;
    drag.start = m_drag_press;
    drag.total = position - m_drag_press;
    if (!m_dragging && left.down && length(drag.total) >= m_theme.drag_threshold)
    {
        m_dragging = true;
        drag.started = true;
    }
    if (m_dragging)
    {
        drag.delta = position - m_drag_last;
        m_drag_last = position;
        drag.dragging = left.down;
        if (!left.down)
        {
            drag.ended = true;
            m_dragging = false;
            m_drag_id = {};
        }
    }
    return drag;
}

void* ImContext::state_slot(ImId id, uint32_t size, const void* tag, bool& created)
{
    require_frame("state");
    if (!is_valid(id))
    {
        throw Error("ImContext state needs a valid id");
    }
    WidgetState& entry = m_state.get(id, m_frame);
    created = entry.tag == nullptr;
    if (created)
    {
        entry.tag = tag;
        entry.size = size;
    }
    else if (entry.tag != tag || entry.size != size)
    {
        throw Error("ImContext state id already holds another type", "give each state type its own id");
    }
    return entry.data;
}

ImId ImContext::item_at(const Vec2f& point) const
{
    for (size_t index = m_last_hits.size(); index-- > 0;)
    {
        if (contains(m_last_hits[index].area, point))
        {
            return m_last_hits[index].id;
        }
    }
    return {};
}

ItemState ImContext::item(ImId id)
{
    Rect rect;
    Rect clip = unbounded_rect();
    std::ignore = layout_rect(id, rect);
    std::ignore = m_layout.clip_of(id, clip);
    return item_clipped(id, rect, clip);
}

uint32_t ImContext::begin_box(std::string_view label, const LayoutStyle& style)
{
    require_frame("begin_box");
    return m_layout.begin_box(id(label), style, m_arena.store(label));
}

uint32_t ImContext::begin_box(ImId id, const LayoutStyle& style)
{
    require_frame("begin_box");
    return m_layout.begin_box(id, style);
}

void ImContext::end_box()
{
    require_frame("end_box");
    m_layout.end_box();
}

bool ImContext::previous_rect(ImId id, Rect& out) const
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
    return { m_frame, m_items, m_draw.command_count(), static_cast<uint32_t>(m_memory.size()), m_arena.used(), m_arena.capacity(), m_layout.node_count(), m_popup_count, static_cast<uint32_t>(m_state.size()), m_arena_high_water,
             m_draw.command_count(DrawKind::Rect), m_draw.command_count(DrawKind::RoundedRect), m_draw.command_count(DrawKind::Border), m_draw.command_count(DrawKind::Line), m_draw.command_count(DrawKind::Text), m_draw.command_count(DrawKind::Image) };
}

std::string dump_layout(const ImContext& context)
{
    return dump_layout(context.layout());
}

} // namespace oryx
