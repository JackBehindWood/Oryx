#pragma once

#include "Oryx/Interface/GUI/Gui.h"

// Draws a DockLayout on the active GuiContext: tab strips, splitters, collapse and close, and a body per panel. The host edits the caller's layout through the DockOps functions and does no file I/O.
// Like every hit area, the host's rects come from last frame's solve of its box, so the first frame draws nothing. Panels are placed by id, so begin_panel calls may come in any order.
namespace oryx::gui
{

// Adds a panel to the context's table; false when the name is taken or the table is full. Register before building a default layout.
bool register_panel(std::string_view name, const PanelOptions& options = {});
[[nodiscard]] const PanelTable& panels();

// Optional docking limits, one call per entry (a panel need not be registered yet as the target): `panel` may then only dock into a node holding one of its dock_only targets,
// and never into a node holding a never_dock target. False when `panel` is unknown, the list (k_max_dock_rules) is full or the entry repeats.
bool dock_only_in(std::string_view panel, std::string_view target);
bool never_dock_in(std::string_view panel, std::string_view target);

// Takes the leftover space like central_area() and draws the layout over it. Every begin_panel_host needs its end_panel_host.
// A layout that fails validate is replaced by the last good one (an empty layout at first) and logged, never thrown.
void begin_panel_host(DockLayout& layout, const PanelHostOptions& options = {});
void end_panel_host();

// The body rect of a viewport panel in the latest frame; empty when it is absent, closed, collapsed or unselected. Valid outside a frame.
[[nodiscard]] Rect viewport_rect(std::string_view name);
[[nodiscard]] PanelHostResult panel_host_result();

// The panel that last took a press; the owner may set it too (the board, a shortcut).
[[nodiscard]] PanelId focused_panel();
void set_focused_panel(PanelId panel);

class PanelHostScope
{
public:
    explicit PanelHostScope(DockLayout& layout, const PanelHostOptions& options = {}) { begin_panel_host(layout, options); }
    ~PanelHostScope() { end_panel_host(); }

    PanelHostScope(const PanelHostScope&) = delete;
    PanelHostScope& operator=(const PanelHostScope&) = delete;
};

}
