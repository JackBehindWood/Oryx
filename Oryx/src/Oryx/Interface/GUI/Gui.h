#pragma once

#include "Oryx/Interface/Canvas/ImWidgets.h"
#include "Oryx/Interface/GUI/GuiContext.h"
#include "Oryx/Interface/GUI/GuiId.h"
#include "Oryx/Interface/GUI/GuiScope.h"

// Begin/end rule (ImGui's): a begin_* that returns bool opens its widget only on true, and its end_* is called only then; calling it otherwise, or an end_* out of order, throws Error, as does a scope left open at end_frame.
// Widgets that always open say so and always end. The *Scope classes (see GuiScope.h) do the pairing and are optional.
// The developer-tooling GUI as free functions over the active GuiContext (see ContextScope<GuiContext>). Each throws Error when none is active; the widgets themselves are the shared ones in oryx::im, so they match oryx::ui.
namespace oryx::gui
{

using im::CanvasOptions;
using im::IconOptions;
using im::ImageButtonOptions;
using im::ImageOptions;
using im::RowOptions;
using im::StatusOptions;
using im::WidgetOptions;

// The active context, for widgets of your own that need more than the free functions; throws Error when none is active.
[[nodiscard]] GuiContext& context();
[[nodiscard]] GuiId id(std::string_view label);
void push_id(std::string_view label);
void pop_id();
// Reports the pointer's relation to a rect and remembers it for the next frame; for widgets of your own.
ItemState item(GuiId id, const Rect& rect);

[[nodiscard]] float spacing();

// What the owner of the surface needs to know, read only. wants_mouse/wants_keyboard say whether the frame's items took the input.
struct GuiIo
{
    bool wants_mouse = false;
    bool wants_keyboard = false;
    float delta_time = 0.0f;
    uint64_t frame = 0;
    uint32_t surface = 0;
    ImPointer pointer;
};

static_assert(std::is_trivially_copyable_v<GuiIo> && std::is_standard_layout_v<GuiIo>);

[[nodiscard]] GuiIo io();
[[nodiscard]] const GuiTheme& theme();
// Applies a whole theme; the dock theme is re-derived from it, so assign set_dock_theme after this for a custom dock look.
void set_theme(const GuiTheme& theme);
// Edit-and-apply over the active theme, each through the free function of the same name in GuiTheme.h, so the roles stay consistent.
void set_theme_accent(const Colour& accent);
void set_theme_text_height(float height);
void set_theme_corner_radius(float radius);
[[nodiscard]] const GuiDockTheme& dock_theme();
void set_dock_theme(const GuiDockTheme& theme);
void request_cursor(CursorShape cursor);
[[nodiscard]] ItemDrag item_drag(GuiId id);
// Widget state under an id of the current scope; see ImContext::state for the lifetime and limits.
template<typename T>
[[nodiscard]] T& state(GuiId id)
{
    return ActiveContext<GuiContext>::require().state<T>(id.im());
}
// The frame of a widget of your own; see im::begin_widget.
[[nodiscard]] ItemState begin_widget(std::string_view name, const LayoutStyle& style);
void end_widget();
// A reserved box to paint into; see im::canvas.
[[nodiscard]] im::CanvasArea canvas(std::string_view name, Sizing width = grow(), Sizing height = grow(), const im::CanvasOptions& options = {});

void label(std::string_view text, const WidgetOptions& options = {});
ItemState button(std::string_view text, const WidgetOptions& options = {});
bool toggle(std::string_view text, bool& value, const WidgetOptions& options = {});
ItemState image(std::string_view name, ImageHandle image, const ImageOptions& options = {});
ItemState image_button(std::string_view name, ImageHandle image, const ImageButtonOptions& options = {});
// Inside an active panel host (see GuiPanelHost.h) and outside any dock panel's body this is a dock panel: it returns whether the panel is visible (selected, expanded, open) and end_panel must follow either way.
// Anywhere else it is a clipped container and returns true.
bool begin_panel(std::string_view name, const PanelOptions& options = {});
void end_panel();
void begin_row(std::string_view name, const RowOptions& options = {});
void end_row();
void begin_column(std::string_view name, const RowOptions& options = {});
void end_column();
void icon(Icon icon, const IconOptions& options = {});
void spacer(float weight = 1.0f);
void separator(const WidgetOptions& options = {});
// Uses the theme's status style unless the options name another.
void status_line(std::string_view text, const StatusOptions& options = {});

// Everything between the calls paints dimmed and ignores the pointer, like ImGui's BeginDisabled; nestable.
void begin_disabled();
void end_disabled();

struct DisabledEnd
{
    static void end() { end_disabled(); }
};

// Disables what is built in its scope when `disabled` is true; a false one does nothing, so it wraps a conditional widget without a branch.
class DisabledScope : public Scope<DisabledEnd>
{
public:
    explicit DisabledScope(bool disabled = true)
        : Scope(begin_if(disabled))
    {
    }

private:
    static bool begin_if(bool disabled)
    {
        if (disabled)
        {
            begin_disabled();
        }
        return disabled;
    }
};

uint32_t begin_box(std::string_view label, const LayoutStyle& style);
void end_box();
// A box floated against the whole surface by one point of itself and the same point of the surface.
[[nodiscard]] LayoutStyle anchored(AttachPoint at, Sizing width = fit(), Sizing height = fit(), const Vec2f& offset = { 0.0f, 0.0f });

struct BoxEnd
{
    static void end() { end_box(); }
};

// Ends the box when it goes out of scope.
class BoxScope : public AlwaysScope<BoxEnd>
{
public:
    BoxScope(std::string_view label, const LayoutStyle& style) { begin_box(label, style); }
};

struct RowEnd
{
    static void end() { end_row(); }
};

class RowScope : public AlwaysScope<RowEnd>
{
public:
    explicit RowScope(std::string_view name, const RowOptions& options = {}) { begin_row(name, options); }
};

struct ColumnEnd
{
    static void end() { end_column(); }
};

class ColumnScope : public AlwaysScope<ColumnEnd>
{
public:
    explicit ColumnScope(std::string_view name, const RowOptions& options = {}) { begin_column(name, options); }
};

struct PanelEnd
{
    static void end() { end_panel(); }
};

class PanelScope : public AlwaysScope<PanelEnd>
{
public:
    explicit PanelScope(std::string_view name, const PanelOptions& options = {})
        : m_visible(begin_panel(name, options))
    {
    }

    [[nodiscard]] bool visible() const { return m_visible; }

private:
    bool m_visible;
};

} // namespace oryx::gui
