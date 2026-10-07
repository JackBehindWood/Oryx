#pragma once

#include "Oryx/Interface/Canvas/ImWidgets.h"
#include "Oryx/Interface/UI/UiContext.h"
#include "Oryx/Interface/UI/UiId.h"

// The player-facing UI as free functions over the active UiContext (see ContextScope<UiContext>). Each throws Error when none is active; the widgets themselves are the shared ones in oryx::im.
namespace oryx::ui
{

using im::RowOptions;
using im::StatusOptions;
using im::WidgetOptions;

[[nodiscard]] UiId id(std::string_view label);
void push_id(std::string_view label);
void pop_id();
// Reports the pointer's relation to a rect and remembers it for the next frame; for widgets of your own.
ItemState item(UiId id, const Rect& rect);

[[nodiscard]] float spacing();

void label(std::string_view text, const WidgetOptions& options = {});
[[nodiscard]] ItemState button(std::string_view text, const WidgetOptions& options = {});
bool toggle(std::string_view text, bool& value, const WidgetOptions& options = {});
void begin_panel(std::string_view name, const WidgetOptions& options = {});
void end_panel();
void begin_row(std::string_view name, const RowOptions& options = {});
void end_row();
void begin_column(std::string_view name, const RowOptions& options = {});
void end_column();
void spacer(float weight = 1.0f);
void separator(const WidgetOptions& options = {});
// Uses the theme's status style unless the options name another.
void status_line(std::string_view text, const StatusOptions& options = {});

uint32_t begin_box(std::string_view label, const LayoutStyle& style);
void end_box();
// A box floated against the whole surface by one point of itself and the same point of the surface.
[[nodiscard]] LayoutStyle anchored(AttachPoint at, Sizing width = fit(), Sizing height = fit(), const Vec2f& offset = { 0.0f, 0.0f });

// Ends the box when it goes out of scope.
class BoxScope
{
public:
    BoxScope(std::string_view label, const LayoutStyle& style) { begin_box(label, style); }
    ~BoxScope() { end_box(); }

    BoxScope(const BoxScope&) = delete;
    BoxScope& operator=(const BoxScope&) = delete;
};

class RowScope
{
public:
    explicit RowScope(std::string_view name, const RowOptions& options = {}) { begin_row(name, options); }
    ~RowScope() { end_row(); }

    RowScope(const RowScope&) = delete;
    RowScope& operator=(const RowScope&) = delete;
};

class ColumnScope
{
public:
    explicit ColumnScope(std::string_view name, const RowOptions& options = {}) { begin_column(name, options); }
    ~ColumnScope() { end_column(); }

    ColumnScope(const ColumnScope&) = delete;
    ColumnScope& operator=(const ColumnScope&) = delete;
};

class PanelScope
{
public:
    explicit PanelScope(std::string_view name, const WidgetOptions& options = {}) { begin_panel(name, options); }
    ~PanelScope() { end_panel(); }

    PanelScope(const PanelScope&) = delete;
    PanelScope& operator=(const PanelScope&) = delete;
};

} // namespace oryx::ui
