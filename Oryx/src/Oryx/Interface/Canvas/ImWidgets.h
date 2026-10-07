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

// The widgets paint when the frame ends and answer from last frame's rects, so a widget that moved reacts one frame late. All throw Error outside a frame.
// The label is the text shown and, under the current id scope, the identity; give repeated labels a scope.

void label(ImContext& context, std::string_view text, const WidgetOptions& options = {});

[[nodiscard]] ItemState button(ImContext& context, std::string_view text, const WidgetOptions& options = {});

// Flips `value` on a click and returns whether it did; the accent colour fills the box while it is on.
bool toggle(ImContext& context, std::string_view text, bool& value, const WidgetOptions& options = {});

// A bordered, filled column that clips its children; close it with end_panel.
void begin_panel(ImContext& context, std::string_view name, const WidgetOptions& options = {});
void end_panel(ImContext& context);

// One line of text floated against the whole surface.
void status_line(ImContext& context, std::string_view text, const StatusOptions& options = {});

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

} // namespace oryx::im
