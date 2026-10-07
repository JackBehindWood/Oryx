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
// `double_clicked` is a click that follows another on the same item within the theme's double_click_seconds; `right_clicked` is the right button released over the item.
struct ItemState
{
    bool hovered = false;
    bool pressed = false;
    bool held = false;
    bool clicked = false;
    bool double_clicked = false;
    bool right_clicked = false;
    // Consecutive seconds the item has been hovered, zero on the first hovered frame; drives tooltip delays.
    float hovered_seconds = 0.0f;
};

// The left-button drag that began on an item. `started` and `ended` are single-frame edges; `delta` is the pointer travel since the previous call, `total` since the press.
struct ItemDrag
{
    bool started = false;
    bool dragging = false;
    bool ended = false;
    Vec2f start{ 0.0f, 0.0f };
    Vec2f delta{ 0.0f, 0.0f };
    Vec2f total{ 0.0f, 0.0f };
};

static_assert(std::is_trivially_copyable_v<ItemState> && std::is_standard_layout_v<ItemState>);
static_assert(std::is_trivially_copyable_v<ItemDrag> && std::is_standard_layout_v<ItemDrag>);

// What the context remembers about an item between frames.
struct ItemMemory
{
    Rect rect;
    Rect previous_rect;
    uint64_t seen_frame = 0;
    uint64_t hovered_frame = 0;
    float hovered_seconds = 0.0f;
    bool has_previous = false;
};

enum class CursorShape : uint8_t
{
    Arrow,
    Hand,
    ResizeHorizontal,
    ResizeVertical,
    Text
};

// What a frame asks of its owner, read after end_frame. Reset by begin_frame.
struct ImOutput
{
    CursorShape cursor = CursorShape::Arrow;
};

static_assert(std::is_trivially_copyable_v<ImOutput> && std::is_standard_layout_v<ImOutput>);

inline constexpr size_t k_max_widget_state_size = 64;

// Reserved draw channels, above the default canvas channel (1): popups, then tooltips, then a dragged ghost.
inline constexpr uint32_t k_channel_popup = 2;
inline constexpr uint32_t k_channel_tooltip = 3;
inline constexpr uint32_t k_channel_drag = 4;

inline constexpr uint32_t k_max_popup_depth = 8;
inline constexpr uint32_t k_max_popups_per_frame = 16;

// Why a popup should close this frame, from last frame's rects (no callbacks): a left press outside it and its nested popups, or Escape while it is the innermost.
struct PopupResult
{
    bool closed_by_outside = false;
    bool closed_by_escape = false;
};

static_assert(std::is_trivially_copyable_v<PopupResult> && std::is_standard_layout_v<PopupResult>);

template<typename T>
struct WidgetStateTag
{
    static inline const char value = 0;
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
    uint32_t popups = 0;
    uint32_t state_entries = 0;
    // The most the frame arena held at the end of any frame so far.
    size_t arena_high_water = 0;
    // `commands` split by kind.
    uint32_t rects = 0;
    uint32_t rounded_rects = 0;
    uint32_t borders = 0;
    uint32_t lines = 0;
    uint32_t texts = 0;
    uint32_t images = 0;
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

    // Hands the frame's wheel to the first caller and zero to every later one, so only the innermost scroll region that asks moves.
    [[nodiscard]] Vec2f consume_wheel();
    [[nodiscard]] bool wheel_consumed() const { return m_wheel_consumed; }

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
    void push_id(ImId id);
    void pop_id();
    [[nodiscard]] ImId current_id() const { return m_id_stack.empty() ? ImId{} : m_id_stack.back(); }
    [[nodiscard]] ImId id(std::string_view label) const { return make_im_id(label, hash_parent()); }
    [[nodiscard]] ImId index_id(uint64_t index) const { return make_im_index_id(index, hash_parent()); }

    // Reports the pointer's relation to the item and remembers its rect for the next frame. The hit area is the rect (grown to the theme's min_hit_size) inside the clip in effect.
    // Throws Error outside a frame, for the none id, or when the id was already used this frame.
    ItemState item(ImId id, const Rect& rect);

    // Same, with the hit area the item's box had in the latest solve (so last frame's while this one is being built), cut by the Clip/Scroll boxes around it; no hit on the first frame.
    ItemState item(ImId id);

    // Boxes nest like ids: end_frame solves them inside `ImInput::surface_size` and paints them into the draw list. The label makes the id under the current scope.
    // Throw Error outside a frame; end_box throws when no box is open. Returns the node index for `layout().node(index)`.
    uint32_t begin_box(std::string_view label, const LayoutStyle& style);
    uint32_t begin_box(ImId id, const LayoutStyle& style);
    void end_box();
    [[nodiscard]] LayoutTree& layout() { return m_layout; }
    [[nodiscard]] const LayoutTree& layout() const { return m_layout; }
    // The rect the box with this id had in the latest solve; false when it was not there.
    [[nodiscard]] bool layout_rect(ImId id, Rect& out) const { return m_layout.rect_of(id, out); }

    // The rect the item had last frame; false when it was not there.
    [[nodiscard]] bool previous_rect(ImId id, Rect& out) const;

    // The drag of the left button that began on `id`, once the pointer has moved past the theme's drag_threshold. Call it once per frame, after item(id); all zeros for any other item.
    // `ended` arrives on the release frame even when the pointer is no longer over the item.
    [[nodiscard]] ItemDrag item_drag(ImId id);

    // The last request of the frame wins, so a widget drawn later (a popup) overrides one below it.
    void request_cursor(CursorShape cursor) { m_output.cursor = cursor; }
    [[nodiscard]] const ImOutput& output() const { return m_output; }

    // Per-id state that outlives frames, for widgets of your own: created value-initialised on first use, dropped after a frame in which it is not asked for.
    // Throws Error when the id holds a different type or size. The reference is valid until the next state call (a new id may move the table); T must be trivially copyable and at most k_max_widget_state_size.
    template<typename T>
    [[nodiscard]] T& state(ImId id)
    {
        static_assert(std::is_trivially_copyable_v<T> && std::is_default_constructible_v<T>, "widget state must be trivially copyable");
        static_assert(sizeof(T) <= k_max_widget_state_size && alignof(T) <= alignof(WidgetState), "widget state too large");
        bool created = false;
        void* slot = state_slot(id, sizeof(T), &WidgetStateTag<T>::value, created);
        if (created)
        {
            new (slot) T{};
        }
        return *static_cast<T*>(slot);
    }

    // Popups: items inside begin_popup_layer/end_popup_layer win over everything beneath, and while the pointer is over last frame's popup rect the items outside any popup are not hovered.
    // `box_id` is the id of the floating box the caller opens for the popup (on k_channel_popup); the result comes from that box's rect in the previous frame. Nested layers form the stack, at most k_max_popup_depth.
    // Throws Error outside a frame, on overflow of the stack or of k_max_popups_per_frame, and end_popup_layer throws with no layer open.
    [[nodiscard]] PopupResult begin_popup_layer(ImId box_id);
    void end_popup_layer();
    [[nodiscard]] uint32_t popup_depth() const { return m_popup_depth; }
    // The box id the innermost open layer was begun with; the none id outside any layer.
    [[nodiscard]] ImId popup_id() const;

    // The topmost item (the last one submitted) whose hit area held the point in the latest finished frame; the none id when there is none. Valid inside a frame too, where it still answers from the frame before.
    [[nodiscard]] ImId item_at(const Vec2f& point) const;

    [[nodiscard]] ImId hot() const { return m_hot; }
    [[nodiscard]] ImId active() const { return m_active; }
    [[nodiscard]] ImId focus() const { return m_focus; }
    // Focus is only reserved here (no navigation); a press that lands on no item clears it.
    void set_focus(ImId id) { m_focus = id; }
    // True while an item is under the pointer or held, so the owner can leave the click to the UI.
    [[nodiscard]] bool wants_mouse() const { return is_valid(m_hot) || is_valid(m_active); }
    [[nodiscard]] bool wants_keyboard() const { return is_valid(m_focus); }

    [[nodiscard]] ImStats stats() const;

protected:
    // The seed salts every id made here, so two context kinds never share an id for the same label.
    explicit ImContext(uint64_t id_seed = 0)
        : m_id_seed(id_seed)
    {
    }
    ~ImContext() = default;

    // Derived contexts keep their own StateTables and collect them with the frame number.
    [[nodiscard]] uint64_t state_frame() const { return m_frame; }

private:
    void require_frame(const char* what) const;
    ItemState item_clipped(ImId id, const Rect& rect, const Rect& layout_clip);
    [[nodiscard]] ImId hash_parent() const { return m_id_stack.empty() ? ImId{ m_id_seed } : m_id_stack.back(); }
    [[nodiscard]] bool under_popup(const Vec2f& point) const;
    void solve_layout();
    void* state_slot(ImId id, uint32_t size, const void* tag, bool& created);

    struct alignas(16) WidgetState
    {
        const void* tag = nullptr;
        uint32_t size = 0;
        alignas(16) std::byte data[k_max_widget_state_size];
    };

    ImInput m_input;
    ImOutput m_output;
    ImTheme m_theme;
    DrawList m_draw;
    LayoutTree m_layout;
    FrameArena m_arena;
    StateTable<ItemMemory> m_memory;
    StateTable<WidgetState> m_state;
    std::vector<ImId> m_id_stack;
    struct ItemHit
    {
        ImId id;
        Rect area;
    };

    struct PopupEntry
    {
        ImId id;
        Rect rect;
        uint32_t depth = 0;
    };

    std::array<PopupEntry, k_max_popups_per_frame> m_popups;
    std::array<PopupEntry, k_max_popups_per_frame> m_last_popups;
    uint32_t m_popup_count = 0;
    uint32_t m_last_popup_count = 0;
    uint32_t m_popup_depth = 0;
    std::vector<ItemHit> m_hits;
    std::vector<ItemHit> m_last_hits;
    ImId m_hot;
    ImId m_active;
    ImId m_focus;
    uint64_t m_id_seed;
    uint64_t m_frame = 0;
    size_t m_arena_high_water = 0;
    float m_time = 0.0f;
    ImId m_last_click_id;
    float m_last_click_time = 0.0f;
    ImId m_drag_id;
    Vec2f m_drag_press{ 0.0f, 0.0f };
    Vec2f m_drag_last{ 0.0f, 0.0f };
    bool m_dragging = false;
    // An item took this frame's press; a later item under the pointer (drawn above it) may still take it over.
    bool m_press_claimed = false;
    uint32_t m_items = 0;
    bool m_open = false;
    bool m_wheel_consumed = false;
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
    IdScope(ImContext& context, ImId id)
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
