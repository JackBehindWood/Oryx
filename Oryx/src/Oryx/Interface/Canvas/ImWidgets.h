#pragma once

#include "Oryx/Interface/Canvas/ImContext.h"

namespace oryx::im
{

// Per-call choices shared by every widget; the defaults come from the context's theme.
struct WidgetOptions
{
    // Names a style variant of the theme ("primary", "danger"); empty or unknown uses the base style.
    std::string_view variant;
    // Replaces the style the variant resolves to. Not owned; read during the call.
    const ImStyle* style = nullptr;
    // Replaces the widget's default box (size, padding, direction). Not owned; read during the call.
    const LayoutStyle* layout = nullptr;
};

struct StatusOptions : WidgetOptions
{
    AttachPoint at = AttachPoint::BottomCentre;
    // Gap between the line and the surface edge it is attached to; negative derives it from the style's padding.
    float margin = -1.0f;
};

// Sugar over LayoutStyle for the common stacks; every form is expressible as a hand-written begin_box.
struct RowOptions
{
    Sizing width = fit();
    Sizing height = fit();
    Insets padding;
    float gap = 0.0f;
    // Placement of the children across the stack's axis.
    Align align = Align::Start;
};

// Pieces the built-in widgets are made of, public so widgets of your own look and behave the same.
// The style an options struct resolves to: its own, else the named variant, else the theme's base.
[[nodiscard]] const ImStyle& resolved_style(const ImContext& context, const WidgetOptions& options);
// The options' layout, else the default box of the style (padding, centred content).
[[nodiscard]] LayoutStyle default_box(const ImStyle& style);
[[nodiscard]] LayoutStyle widget_box(const ImStyle& style, const WidgetOptions& options);
void paint_text(BoxPaint& paint, const LayoutNode& node, const ImStyle& style, TextAlign align, bool ellipsis);
void paint_surface(BoxPaint& paint, const ImStyle& style, const Colour& fill);
[[nodiscard]] Colour interaction_fill(const ImStyle& style, const ItemState& state, const Colour& rest);
// A leaf box that reacts to the pointer; `rest` (null for the style's background) shows while idle.
ItemState interactive_box(ImContext& context, std::string_view text, const WidgetOptions& options, const Colour* rest);

// Where a popup opens, from its size last frame (zero on the first, so it opens unflipped): below the anchor box with the left edges aligned, above and/or right-aligned when it would leave the surface.
[[nodiscard]] Floating popup_below(ImId anchor, const Rect& anchor_rect, const Vec2f& last_size, const Vec2f& surface_size);
// At a point such as the pointer, `gap` away from it, pushed to the other side when it would leave the surface.
[[nodiscard]] Floating popup_at(const Vec2f& point, const Vec2f& last_size, const Vec2f& surface_size, float gap = 0.0f);

// The widgets paint when the frame ends and answer from last frame's rects, so a widget that moved reacts one frame late. All throw Error outside a frame.
// The label is the text shown and, under the current id scope, the identity; give repeated labels a scope.

void label(ImContext& context, std::string_view text, const WidgetOptions& options = {});

[[nodiscard]] ItemState button(ImContext& context, std::string_view text, const WidgetOptions& options = {});

// Flips `value` on a click and returns whether it did; the accent colour fills the box while it is on.
bool toggle(ImContext& context, std::string_view text, bool& value, const WidgetOptions& options = {});

// A bordered, filled column that clips its children; close it with end_panel.
void begin_panel(ImContext& context, std::string_view name, const WidgetOptions& options = {});
void end_panel(ImContext& context);

// A box with no paint whose children flow left to right (row) or top to bottom (column); close it with the matching end. The name scopes the children's ids.
void begin_row(ImContext& context, std::string_view name, const RowOptions& options = {});
void end_row(ImContext& context);
void begin_column(ImContext& context, std::string_view name, const RowOptions& options = {});
void end_column(ImContext& context);

// Takes the leftover space of the stack it sits in, shared by weight.
void spacer(ImContext& context, float weight = 1.0f);

// A one-pixel-or-border-wide line across the stack it sits in, in the style's border colour.
void separator(ImContext& context, const WidgetOptions& options = {});

// One line of text floated against the whole surface.
void status_line(ImContext& context, std::string_view text, const StatusOptions& options = {});

// The frame of a widget of your own: reports the item under `name` (from last frame's box), opens the box and scopes the ids inside it. Close it with end_widget.
[[nodiscard]] ItemState begin_widget(ImContext& context, std::string_view name, const LayoutStyle& style);
void end_widget(ImContext& context);

struct CanvasOptions
{
    // Z-order channel the painting goes to; above the boxes' own paint at the default.
    uint32_t channel = 1;
};

// A reserved box that hit-tests, clips and hands out a Painter. Painting is recorded at once, over the box's last-frame rect (so a moved canvas paints one frame late); it ends when the area is destroyed.
// `drag` is item_drag for the canvas and `pointer_local` the pointer relative to the rect's top-left, both from the same frame's input.
class CanvasArea
{
public:
    CanvasArea(const CanvasArea&) = delete;
    CanvasArea& operator=(const CanvasArea&) = delete;
    ~CanvasArea();

    Rect rect;
    ItemState item;
    ItemDrag drag;
    Vec2f pointer_local{ 0.0f, 0.0f };
    Vec2f wheel{ 0.0f, 0.0f };
    Painter painter;

private:
    friend CanvasArea canvas(ImContext&, std::string_view, Sizing, Sizing, const CanvasOptions&);
    CanvasArea(ImContext& context, Painter&& painter, uint32_t previous_channel, const Rect& rect, const ItemState& item, const ItemDrag& drag, const Vec2f& pointer_local, const Vec2f& wheel)
        : rect(rect)
        , item(item)
        , drag(drag)
        , pointer_local(pointer_local)
        , wheel(wheel)
        , painter(std::move(painter))
        , m_draw(context.draw_list())
        , m_previous_channel(previous_channel)
    {
    }

    DrawList& m_draw;
    uint32_t m_previous_channel;
};

// Throws Error outside a frame or while the theme has no font. The wheel is only taken when the pointer is over the canvas.
[[nodiscard]] CanvasArea canvas(ImContext& context, std::string_view name, Sizing width = grow(), Sizing height = grow(), const CanvasOptions& options = {});

// Ends the panel when it goes out of scope.
class PanelScope
{
public:
    PanelScope(ImContext& context, std::string_view name, const WidgetOptions& options = {})
        : m_context(context)
    {
        begin_panel(m_context, name, options);
    }
    ~PanelScope() { end_panel(m_context); }

    PanelScope(const PanelScope&) = delete;
    PanelScope& operator=(const PanelScope&) = delete;

private:
    ImContext& m_context;
};

class RowScope
{
public:
    RowScope(ImContext& context, std::string_view name, const RowOptions& options = {})
        : m_context(context)
    {
        begin_row(m_context, name, options);
    }
    ~RowScope() { end_row(m_context); }

    RowScope(const RowScope&) = delete;
    RowScope& operator=(const RowScope&) = delete;

private:
    ImContext& m_context;
};

class ColumnScope
{
public:
    ColumnScope(ImContext& context, std::string_view name, const RowOptions& options = {})
        : m_context(context)
    {
        begin_column(m_context, name, options);
    }
    ~ColumnScope() { end_column(m_context); }

    ColumnScope(const ColumnScope&) = delete;
    ColumnScope& operator=(const ColumnScope&) = delete;

private:
    ImContext& m_context;
};

} // namespace oryx::im
