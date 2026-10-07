#pragma once

#include "Oryx/Interface/GUI/Gui.h"

// The developer-tooling widget set. Like everything in oryx::gui they answer from last frame's rects, take a std::string_view label that is also the identity under the current id scope,
// and return plain result structs. Labelled controls put the label on the side the theme (or the call) says, see FieldOptions.
namespace oryx::gui
{

inline constexpr uint32_t k_no_index = std::numeric_limits<uint32_t>::max();

// Options of every labelled control: the label's side and width default to the theme's.
struct FieldOptions : WidgetOptions
{
    LabelSide label_side = LabelSide::Default;
    // Negative takes the theme's label_width; zero fits the text.
    float label_width = -1.0f;
    Sizing width = grow();
};

static_assert(std::is_trivially_copyable_v<FieldOptions>);

// A row holding a label and one control, built in the order the options and the theme choose; the row is the box named `name` and scopes the ids of its control.
// Open it, add the control's boxes, close it by leaving the scope.
class FieldScope
{
public:
    FieldScope(std::string_view name, const FieldOptions& options);
    ~FieldScope();

    FieldScope(const FieldScope&) = delete;
    FieldScope& operator=(const FieldScope&) = delete;

private:
    std::string_view m_name;
    const ImStyle* m_style;
    float m_label_width;
    bool m_label_after;
};

// A leaf box with one line of text; the text is copied into the frame arena. Returns the node index.
uint32_t text_box(std::string_view text, const ImStyle& style, Sizing width = fit(), TextAlign align = TextAlign::Left);
// A floating box covering its parent with centred text, for values drawn over a track.
void overlay_text(std::string_view text, const ImStyle& style);

// Basics

void text_coloured(std::string_view text, const Colour& colour, const WidgetOptions& options = {});
void bullet(std::string_view text, const WidgetOptions& options = {});
// A row with the key on the left and the value on the right, for statistics.
void key_value(std::string_view key, std::string_view value, const WidgetOptions& options = {});
[[nodiscard]] ItemState small_button(std::string_view text, const WidgetOptions& options = {});
// Flips `value` on a click; returns whether it did. The whole row, label included, is the click area.
bool checkbox(std::string_view label, bool& value, const FieldOptions& options = {});
// Sets `value` to `option` on a click; returns whether it changed.
bool radio(std::string_view label, int32_t& value, int32_t option, const FieldOptions& options = {});

struct SelectableOptions : WidgetOptions
{
    // Spans the stack it sits in (list rows) instead of fitting its text.
    bool full_width = true;
};

static_assert(std::is_trivially_copyable_v<SelectableOptions>);

[[nodiscard]] ItemState selectable(std::string_view label, bool selected, const SelectableOptions& options = {});
void badge(std::string_view text, const Colour& colour, const WidgetOptions& options = {});
void colour_swatch(std::string_view label, const Colour& colour, const FieldOptions& options = {});
// `fraction` is clamped to 0..1; `text` is drawn over the bar when not empty.
void progress(std::string_view label, float fraction, std::string_view text = {}, const FieldOptions& options = {});

// Structure

// Returns whether the header is open; the content follows at the same level. The open flag lives in widget state under the header's id.
[[nodiscard]] bool collapsing_header(std::string_view label, bool default_open = false, const WidgetOptions& options = {});

// With a `visible` pointer the header gets a close button at its right edge, like ImGui's `p_open`: the click clears *visible, and while it is false nothing is drawn and the result is false,
// so the body (and everything it would compute) is skipped for a closed panel. The caller owns the flag and reopens it from a menu or toggle. A null pointer is the plain header.
[[nodiscard]] bool collapsing_header(std::string_view label, bool* visible, bool default_open = false, const WidgetOptions& options = {});

struct TreeNodeOptions : WidgetOptions
{
    bool default_open = false;
    // No children and no toggle: a row that can only be selected.
    bool leaf = false;
    bool selected = false;
};

static_assert(std::is_trivially_copyable_v<TreeNodeOptions>);

struct TreeNodeResult
{
    bool open = false;
    ItemState item;
};

static_assert(std::is_trivially_copyable_v<TreeNodeResult> && std::is_standard_layout_v<TreeNodeResult>);

// A row that toggles on a click; while open its children go in an indented column and end_tree_node closes it (call it only when `open`). The label scopes the children's ids.
[[nodiscard]] TreeNodeResult begin_tree_node(std::string_view label, const TreeNodeOptions& options = {});
void end_tree_node();

class TreeScope
{
public:
    explicit TreeScope(std::string_view label, const TreeNodeOptions& options = {})
        : m_result(begin_tree_node(label, options))
    {
    }
    ~TreeScope()
    {
        if (m_result.open)
        {
            end_tree_node();
        }
    }

    TreeScope(const TreeScope&) = delete;
    TreeScope& operator=(const TreeScope&) = delete;

    [[nodiscard]] bool open() const { return m_result.open; }
    [[nodiscard]] const ItemState& item() const { return m_result.item; }

private:
    TreeNodeResult m_result;
};

struct ScrollOptions : WidgetOptions
{
    Sizing width = grow();
    Sizing height = grow();
    float gap = 0.0f;
};

static_assert(std::is_trivially_copyable_v<ScrollOptions>);

// A clipped column that scrolls vertically with the wheel and a draggable bar. The offset lives in widget state under the name and is re-clamped every frame against the last solve.
// The wheel goes to the innermost hovered region that can still move that way, so a region at its end passes it on to the one around it.
void begin_scroll(std::string_view name, const ScrollOptions& options = {});
void end_scroll();
// The offset of the region `name` would have in the current id scope, in pixels from the top; for widgets that draw only what is visible.
[[nodiscard]] float scroll_offset(std::string_view name);

class ScrollScope
{
public:
    explicit ScrollScope(std::string_view name, const ScrollOptions& options = {}) { begin_scroll(name, options); }
    ~ScrollScope() { end_scroll(); }

    ScrollScope(const ScrollScope&) = delete;
    ScrollScope& operator=(const ScrollScope&) = delete;
};

struct TabBarOptions : WidgetOptions
{
    bool closable = false;
};

static_assert(std::is_trivially_copyable_v<TabBarOptions>);

// `pressed_index` is the tab pressed this frame and `drag_index` the one being dragged (with `drag`), both k_no_index otherwise; closed_index is a closed tab's index. The caller owns the order and the content.
struct TabBarResult
{
    bool changed = false;
    uint32_t pressed_index = k_no_index;
    uint32_t closed_index = k_no_index;
    uint32_t drag_index = k_no_index;
    ItemDrag drag;
};

static_assert(std::is_trivially_copyable_v<TabBarResult> && std::is_standard_layout_v<TabBarResult>);

// A strip of tabs, one tab() call each in order; a press selects. `selected` is the caller's, so tab content is a plain `if` on it once the scope ends.
class TabBarScope
{
public:
    TabBarScope(std::string_view name, uint32_t& selected, const TabBarOptions& options = {});
    ~TabBarScope();

    TabBarScope(const TabBarScope&) = delete;
    TabBarScope& operator=(const TabBarScope&) = delete;

    // True while this tab is the selected one (after this frame's press).
    bool tab(std::string_view label);
    // A tab with its own close button, like ImGui's TabItem `p_open`: the click clears *open, and while it is false nothing is drawn and the result is false (skip the content too).
    // The index still counts, so the other tabs keep theirs.
    bool tab(std::string_view label, bool* open);
    // What the tabs added so far reported.
    [[nodiscard]] const TabBarResult& result() const { return m_result; }

private:
    bool draw_tab(std::string_view label, bool closable, bool* open);

    uint32_t& m_selected;
    const ImStyle* m_style;
    bool m_closable;
    uint32_t m_count = 0;
    TabBarResult m_result;
};

struct SplitterOptions
{
    // Row: the panes sit side by side and the divider drags horizontally. Column: stacked, vertical drag.
    Direction axis = Direction::Row;
    float min_first = 40.0f;
    float min_second = 40.0f;
    float thickness = 4.0f;
};

static_assert(std::is_trivially_copyable_v<SplitterOptions> && std::is_standard_layout_v<SplitterOptions>);

// A divider between two panes of the stack it sits in. The caller sizes the first pane `fixed(first_size)` and lets the second grow; returns whether `first_size` changed.
bool splitter(std::string_view name, float& first_size, const SplitterOptions& options = {});

struct ListBoxOptions : WidgetOptions
{
    Sizing height = fixed(120.0f);
    // Nonzero virtualises the list: the scroll range spans item_count rows of item_height, and the caller submits only first_item()..last_item() with item(index, ...).
    uint32_t item_count = 0;
    float item_height = 22.0f;
};

static_assert(std::is_trivially_copyable_v<ListBoxOptions>);

// Selectable rows in a scroll region; the caller owns the selection. Without item_count, one item(label, selected) call per row. With it, a long list costs only its visible rows:
//   ListBoxScope list("games", { .item_count = n }); for (i = list.first_item(); i < list.last_item(); ++i) { if (list.item(i, names[i], i == selected).clicked) ... }
// The range comes from last frame's scroll offset and view height with a row of margin each side, so it is one frame late like every hit area.
class ListBoxScope
{
public:
    explicit ListBoxScope(std::string_view name, const ListBoxOptions& options = {});
    ~ListBoxScope();

    ListBoxScope(const ListBoxScope&) = delete;
    ListBoxScope& operator=(const ListBoxScope&) = delete;

    // A row; the state's `clicked` says it was picked. Throws Error on a virtualised list, which needs the index.
    [[nodiscard]] ItemState item(std::string_view label, bool selected);
    [[nodiscard]] ItemState item(uint32_t index, std::string_view label, bool selected);
    [[nodiscard]] uint32_t first_item() const { return m_first; }
    [[nodiscard]] uint32_t last_item() const { return m_last; }

private:
    struct Range
    {
        uint32_t first = 0;
        uint32_t last = 0;
    };

    ListBoxScope(std::string_view name, const ListBoxOptions& options, const Range& range);
    [[nodiscard]] static Range visible_range(std::string_view name, const ListBoxOptions& options);
    void spacer(uint32_t rows);

    uint32_t m_first = 0;
    uint32_t m_last = 0;
    ScrollScope m_scroll;
    ListBoxOptions m_options;
    LayoutStyle m_row;
    uint32_t m_count = 0;
};

// True when every character of `needle` matches `text` as a case-insensitive (ASCII) substring; an empty needle matches everything. The needle is fed by the caller.
[[nodiscard]] bool filter_matches(std::string_view needle, std::string_view text);

// Input

struct SliderOptions : FieldOptions
{
    // Change per arrow key when focused; zero uses a hundredth of the range (one for integers).
    float step = 0.0f;
};

static_assert(std::is_trivially_copyable_v<SliderOptions>);

bool slider_float(std::string_view label, float& value, float min, float max, const SliderOptions& options = {});
bool slider_int(std::string_view label, int32_t& value, int32_t min, int32_t max, const SliderOptions& options = {});

struct DragOptions : FieldOptions
{
    // Value change per pixel dragged; also the change per arrow key when focused.
    float speed = 1.0f;
};

static_assert(std::is_trivially_copyable_v<DragOptions>);

// Dragging sideways changes the value; there is no text entry. A min not below max leaves the value unclamped.
bool drag_float(std::string_view label, float& value, float min = 0.0f, float max = 0.0f, const DragOptions& options = {});
bool drag_int(std::string_view label, int32_t& value, int32_t min = 0, int32_t max = 0, const DragOptions& options = {});

// A button showing `preview` that opens a list while the scope lives; one item() call per entry, the caller owns the selection and the preview text. Closes on a pick, a press outside or Escape.
class ComboScope
{
public:
    ComboScope(std::string_view label, std::string_view preview, const FieldOptions& options = {});
    ~ComboScope();

    ComboScope(const ComboScope&) = delete;
    ComboScope& operator=(const ComboScope&) = delete;

    [[nodiscard]] bool open() const { return m_popup; }
    // An entry of the open list; true on the frame it is picked. Does nothing while the list is closed.
    bool item(std::string_view text, bool selected);

private:
    GuiContext& m_context;
    FieldScope m_field;
    ImId m_control;
    ItemState m_state;
    bool m_open;
    bool m_popup = false;
    PopupResult m_result;
    uint32_t m_count = 0;
};

} // namespace oryx::gui
