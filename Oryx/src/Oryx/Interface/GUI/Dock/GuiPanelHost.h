#pragma once

#include "Oryx/Interface/GUI/Gui.h"

// Draws the active GuiContext's dock: tab strips, splitters, collapse and close, and a body per panel. The context owns the layout, its file, history and focus; callers say what they want by panel name.
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

// Takes the leftover space like central_area() and draws the context's dock over it. The context owns the layout: the first call (or the first layout query) loads `gui.layout_file` or builds the default from the registered hints,
// autosaves reactively and writes a pending change when the context is destroyed; an empty layout_file keeps everything in memory. With `gui.docking` off the Viewport panel fills the host and nothing else is drawn.
// Every begin_panel_host needs its end_panel_host. A layout that fails validate is replaced by the last good one and logged, never thrown. Register panels before the first call.
void begin_panel_host();
void end_panel_host();

// The body rect of a viewport panel in the latest frame; empty when it is absent, closed, collapsed or unselected. Valid outside a frame.
[[nodiscard]] Rect viewport_rect(std::string_view name);
// The body rect of any visible panel in the latest frame (floats included); empty when absent, closed, collapsed or unselected. For a PanelOptions::foreign_body panel another context draws into it.
[[nodiscard]] Rect panel_rect(std::string_view name);
// True when a float drawn above panel `name` (all of them for a docked panel) covered `point` last frame, so a second context beneath the host should not take the pointer there. Popups are not counted: popup_open() says that.
[[nodiscard]] bool panel_occluded(std::string_view name, const Vec2f& point);
// For a foreign_body panel: the host hides the pointer from the other context (a float over the panel at the pointer, or an open popup).
[[nodiscard]] bool panel_blocked(std::string_view name);
// For a foreign_body panel: its body is visible and the other context wants the pointer / keyboard that the host must not hand on.
[[nodiscard]] bool panel_claims_pointer(std::string_view name, const GuiContext& other);
[[nodiscard]] bool panel_claims_keyboard(std::string_view name, const GuiContext& other);
[[nodiscard]] PanelHostResult panel_host_result();

// The toolbar strip reserved for the panel being drawn (PanelOptions::toolbar); false, with nothing to close, when the panel has none, is hidden or this is not a docked panel. Call between begin_panel and end_panel.
[[nodiscard]] bool begin_panel_toolbar();
void end_panel_toolbar();
// The strip in the latest frame; empty when absent. Valid outside a frame.
[[nodiscard]] Rect panel_toolbar_rect(std::string_view name);

// Panels by name. Unknown names are answered with false / UnknownPanel, never thrown. Edits obey the panels' flags and docking rules, are undoable and are saved by the same reactive autosave.
[[nodiscard]] bool is_panel_open(std::string_view name);
DockResult set_panel_open(std::string_view name, bool open);
DockResult toggle_panel(std::string_view name);
// The panel that last took a press (a viewport press included); the owner may set it too (a shortcut).
[[nodiscard]] bool is_panel_focused(std::string_view name);
void focus_panel(std::string_view name);

// Groups are named at registration (PanelOptions::group). SessionOnly changes the layout and the default without saving it or adding an undo step (a command-line flag); User is an ordinary edit.
enum class GroupEdit : uint8_t
{
    User,
    SessionOnly
};

[[nodiscard]] bool is_group_open(std::string_view group);
DockResult set_group_open(std::string_view group, bool open, GroupEdit edit = GroupEdit::User);

// Single edits by name: the same operations a drag makes. An empty `target` docks beside the whole tree; `target` otherwise names a panel whose tab stack is the target.
DockResult dock_panel(std::string_view panel, std::string_view target, DropZone zone);
DockResult float_panel(std::string_view panel, const Rect& rect);
DockResult select_panel(std::string_view panel);
DockResult set_panel_collapsed(std::string_view panel, bool collapsed);

// Whole layouts. set_default_layout is what Reset layout and a first run use (it replaces the hint-built default; call before the first host frame); set_dock_layout replaces the live layout as one undo step. Both validate and return false, logged, on a bad layout.
bool set_default_layout(const DockLayout& layout);
bool set_dock_layout(const DockLayout& layout);
[[nodiscard]] const DockLayout& dock_layout();
void reset_layout();

// Layout history: one entry per finished change (a drag, a close, a Panels-menu toggle), none while a splitter or panel is held. The request is applied at the next begin_panel_host and ignored mid-drag.
void undo_layout();
void redo_layout();
[[nodiscard]] bool can_undo_layout();
[[nodiscard]] bool can_redo_layout();

// Panels list, Reset layout and Undo/Redo layout as items of the menu being built; nothing while docking is off.
void dock_menu();

class PanelToolbarScope
{
public:
    PanelToolbarScope() : m_open(begin_panel_toolbar()) {}
    ~PanelToolbarScope()
    {
        if (m_open)
            end_panel_toolbar();
    }

    [[nodiscard]] bool visible() const { return m_open; }

    PanelToolbarScope(const PanelToolbarScope&) = delete;
    PanelToolbarScope& operator=(const PanelToolbarScope&) = delete;

private:
    bool m_open;
};

class PanelHostScope
{
public:
    PanelHostScope() { begin_panel_host(); }
    ~PanelHostScope() { end_panel_host(); }

    PanelHostScope(const PanelHostScope&) = delete;
    PanelHostScope& operator=(const PanelHostScope&) = delete;
};

}
