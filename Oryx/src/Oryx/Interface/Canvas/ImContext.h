#pragma once

#include "Oryx/Interface/Canvas/DrawList.h"
#include "Oryx/Interface/Canvas/FrameArena.h"
#include "Oryx/Interface/Canvas/ImInput.h"
#include "Oryx/Interface/Canvas/ImTheme.h"
#include "Oryx/Interface/Canvas/LayoutTree.h"
#include "Oryx/Interface/Canvas/Painter.h"
#include "Oryx/Interface/Canvas/StateTable.h"

namespace oryx
{

// What one widget interaction reports. `pressed` is the frame the button went down on the item, `clicked` the frame it was released over the item that it went down on.
struct ItemState
{
    bool hovered = false;
    bool pressed = false;
    bool held = false;
    bool clicked = false;
};

// What the context remembers about an item between frames.
struct ItemMemory
{
    Rect rect;
    Rect previous_rect;
    uint64_t seen_frame = 0;
    bool has_previous = false;
};

// Counters of the last frame, for tests and benchmarks.
struct ImStats
{
    uint64_t frame = 0;
    uint32_t items = 0;
    uint32_t commands = 0;
    uint32_t remembered_items = 0;
    size_t arena_used = 0;
    size_t arena_capacity = 0;
    uint32_t boxes = 0;
};

// The state both the player-facing and the tooling immediate-mode systems share: input, ids, hot/active/focus, a frame arena, the DrawList being recorded and the theme.
// Not polymorphic: UiContext and GuiContext hold one by inheritance and add their own state. One context per surface; hot/active are per context. Main thread only.
class ImContext
{
public:
    ImContext(const ImContext&) = delete;
    ImContext& operator=(const ImContext&) = delete;

    // Throws Error when a frame is already open (abort_frame recovers after a thrown error).
    void begin_frame(const ImInput& input);
    // Throws Error with no open frame, or with an unbalanced id or clip stack.
    void end_frame();
    // Drops the open frame without checks so the next begin_frame can start clean.
    void abort_frame();
    [[nodiscard]] bool frame_open() const { return m_open; }
    [[nodiscard]] uint64_t frame() const { return m_frame; }

    [[nodiscard]] const ImInput& input() const { return m_input; }
    [[nodiscard]] float delta_time() const { return m_input.delta_time; }

    // Copies the shared part of a derived theme; the derived roles stay with the derived context.
    void set_theme(const ImTheme& theme) { m_theme = theme; }
    [[nodiscard]] const ImTheme& theme() const { return m_theme; }

    [[nodiscard]] DrawList& draw_list() { return m_draw; }
    [[nodiscard]] const DrawList& draw_list() const { return m_draw; }
    // Throws Error while the theme has no font.
    [[nodiscard]] Painter painter(float scale = 1.0f);
    [[nodiscard]] FrameArena& arena() { return m_arena; }

    // Ids nest: an id made inside a scope is unique to it. Throw Error outside a frame; pop_id throws when the stack is empty.
    void push_id(std::string_view label);
    void push_id(Id id);
    void pop_id();
    [[nodiscard]] Id current_id() const { return m_id_stack.empty() ? Id{} : m_id_stack.back(); }
    [[nodiscard]] Id id(std::string_view label) const { return make_id(label, current_id()); }
    [[nodiscard]] Id index_id(uint64_t index) const { return make_index_id(index, current_id()); }

    // Reports the pointer's relation to the item and remembers its rect for the next frame. The hit area is the rect (grown to the theme's min_hit_size) inside the clip in effect.
    // Throws Error outside a frame, for the none id, or when the id was already used this frame.
    ItemState item(Id id, const Rect& rect);

    // Same, with the hit area the item's box had in the latest solve (so last frame's while this one is being built); no hit on the first frame.
    ItemState item(Id id);

    // Boxes nest like ids: end_frame solves them inside `ImInput::surface_size` and paints them into the draw list. The label makes the id under the current scope.
    // Throw Error outside a frame; end_box throws when no box is open. Returns the node index for `layout().node(index)`.
    uint32_t begin_box(std::string_view label, const LayoutStyle& style);
    uint32_t begin_box(Id id, const LayoutStyle& style);
    void end_box();
    [[nodiscard]] LayoutTree& layout() { return m_layout; }
    [[nodiscard]] const LayoutTree& layout() const { return m_layout; }
    // The rect the box with this id had in the latest solve; false when it was not there.
    [[nodiscard]] bool layout_rect(Id id, Rect& out) const { return m_layout.rect_of(id, out); }

    // The rect the item had last frame; false when it was not there.
    [[nodiscard]] bool previous_rect(Id id, Rect& out) const;

    [[nodiscard]] Id hot() const { return m_hot; }
    [[nodiscard]] Id active() const { return m_active; }
    [[nodiscard]] Id focus() const { return m_focus; }
    // Focus is only reserved here (no navigation); a press that lands on no item clears it.
    void set_focus(Id id) { m_focus = id; }
    // True while an item is under the pointer or held, so the owner can leave the click to the UI.
    [[nodiscard]] bool wants_mouse() const { return is_valid(m_hot) || is_valid(m_active); }
    [[nodiscard]] bool wants_keyboard() const { return is_valid(m_focus); }

    [[nodiscard]] ImStats stats() const;

protected:
    ImContext() = default;
    ~ImContext() = default;

    // Derived contexts keep their own StateTables and collect them with the frame number.
    [[nodiscard]] uint64_t state_frame() const { return m_frame; }

private:
    void require_frame(const char* what) const;
    void solve_layout();

    ImInput m_input;
    ImTheme m_theme;
    DrawList m_draw;
    LayoutTree m_layout;
    FrameArena m_arena;
    StateTable<ItemMemory> m_memory;
    std::vector<Id> m_id_stack;
    std::vector<uint64_t> m_frame_items;
    Id m_hot;
    Id m_active;
    Id m_focus;
    uint64_t m_frame = 0;
    uint32_t m_items = 0;
    bool m_open = false;
};

// The box tree of a context as text, for golden tests.
[[nodiscard]] std::string dump_layout(const ImContext& context);

// Pushes an id scope for its lifetime.
class IdScope
{
public:
    IdScope(ImContext& context, std::string_view label)
        : m_context(context)
    {
        m_context.push_id(label);
    }
    IdScope(ImContext& context, Id id)
        : m_context(context)
    {
        m_context.push_id(id);
    }
    ~IdScope() { m_context.pop_id(); }

    IdScope(const IdScope&) = delete;
    IdScope& operator=(const IdScope&) = delete;

private:
    ImContext& m_context;
};

} // namespace oryx
