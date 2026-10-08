#pragma once

#include "Oryx/Interface/GUI/GuiControls.h"

// Carves the surface into a menu bar, side panels and a central area for the application's own content (the way ImGui's work rect and a game viewport widget do), with no rect arithmetic.
// Declaration order decides placement; after end_frame GuiContext::workspace() reports the solved areas.
namespace oryx::gui
{

enum class Side : uint8_t
{
    Left,
    Right
};

// A column filling the surface. Inside it: begin_menu_bar/end_menu_bar, then begin_body/end_body.
void begin_workspace(std::string_view name);
void end_workspace();

// A row below the menu bar that takes the rest of the surface; holds the side panels and central_area in the order they sit on screen.
void begin_body();
void end_body();

// Reserves the leftover space for the application's own content and draws nothing. Read it back with GuiContext::workspace().central.
void central_area();

// A full-height panel of `width` points (at most half the surface) that clips and scrolls its content.
void begin_side_panel(Side side, float width, const WidgetOptions& options = {});
void end_side_panel();

class WorkspaceScope
{
public:
    explicit WorkspaceScope(std::string_view name) { begin_workspace(name); }
    ~WorkspaceScope() { end_workspace(); }

    WorkspaceScope(const WorkspaceScope&) = delete;
    WorkspaceScope& operator=(const WorkspaceScope&) = delete;
};

class BodyScope
{
public:
    BodyScope() { begin_body(); }
    ~BodyScope() { end_body(); }

    BodyScope(const BodyScope&) = delete;
    BodyScope& operator=(const BodyScope&) = delete;
};

class SidePanelScope
{
public:
    SidePanelScope(Side side, float width, const WidgetOptions& options = {}) { begin_side_panel(side, width, options); }
    ~SidePanelScope() { end_side_panel(); }

    SidePanelScope(const SidePanelScope&) = delete;
    SidePanelScope& operator=(const SidePanelScope&) = delete;
};

} // namespace oryx::gui
