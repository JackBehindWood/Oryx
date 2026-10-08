#pragma once

#include "Oryx/Interface/Canvas/ImWidgets.h"
#include "Oryx/Interface/GUI/Dock/DockOps.h"
#include "Oryx/Interface/GUI/Dock/DockSolve.h"

// The data behind the panel host: options, the per-context state and the frame's body table. Behaviour lives in GuiPanelHost.h; GuiContext owns one PanelHostState.
namespace oryx::gui
{

inline constexpr uint32_t k_max_panel_stack = 8;
inline constexpr uint32_t k_max_reported_panels = 16;

// ImGui-style opt-outs: every panel docks, closes, collapses, resizes, reorders and tears off unless a flag says otherwise. Outside a panel host only the WidgetOptions part is read.
struct PanelOptions : im::WidgetOptions
{
    // Used when the name is registered implicitly; empty uses the name.
    std::string_view title;
    PanelKind kind = PanelKind::View;
    float min_w = 0.0f;
    float min_h = 0.0f;
    bool no_dock = false;
    bool no_close = false;
    bool no_collapse = false;
    bool no_resize = false;
    bool no_reorder = false;
    bool no_tear_off = false;
    // Reserves a toolbar strip drawn with begin_panel_toolbar; where it sits against the tabs follows DockStyle, then this placement, then the node.
    bool toolbar = false;
    ToolbarPlacement toolbar_placement = ToolbarPlacement::Auto;
};

static_assert(std::is_trivially_copyable_v<PanelOptions>);

struct PanelHostOptions
{
    // Checked by validate: a layout without a Viewport panel is rejected.
    bool require_viewport = false;
    // When set, an empty surface offers a Reset button that copies it over the layout. Read during the call.
    const DockLayout* default_layout = nullptr;
    DockStyle style;
};

// What the last host frame did to the layout, for the owner to push history on release or debounce a save.
struct PanelHostResult
{
    bool layout_changed = false;
    // A splitter is held.
    bool interacting = false;
    // A tab or float is being dragged.
    bool dragging = false;
};

static_assert(std::is_trivially_copyable_v<PanelHostResult> && std::is_standard_layout_v<PanelHostResult>);

// A panel's body in the frame's solve; viewport bodies draw nothing and only report their rect.
struct PanelBody
{
    PanelId panel;
    Rect rect;
    // Empty when the panel has no toolbar or it is hidden.
    Rect toolbar;
    uint32_t channel = 0;
    bool viewport = false;
};

enum class PanelCall : uint8_t
{
    Plain,
    Dock,
    DockFloat,
    DockHidden
};

enum class DragSource : uint8_t
{
    None,
    Tab,
    Float
};

// The one drag in flight: plain values, so it needs no allocation and survives a rebuilt tree.
struct DragState
{
    DragSource source = DragSource::None;
    PanelId panel;
    // Pointer minus the dragged tab's or float's top-left corner when the drag began.
    Vec2f grab{ 0.0f, 0.0f };
    DropPlan plan;
};

enum class HistoryRequest : uint8_t
{
    None,
    Undo,
    Redo
};

struct PanelHostState
{
    PanelTable panels;
    DockLayout last_good;
    bool has_good = false;
    PanelId focused;
    uint32_t reported[k_max_reported_panels] = {};
    uint32_t reported_count = 0;

    bool in_host = false;
    uint64_t frame = 0;
    bool dock_body_open = false;
    DockLayout* layout = nullptr;
    PanelHostOptions options;
    ImId host_id;
    Rect host_rect;
    SolvedLayout solved;
    PanelBody bodies[k_max_panels];
    uint32_t body_count = 0;
    PanelHostResult result;
    PanelCall stack[k_max_panel_stack] = {};
    uint32_t depth = 0;
    int32_t current_body = -1;
    bool toolbar_open = false;
    DragState drag;
    LayoutHistory history;
    bool history_seeded = false;
    HistoryRequest history_request = HistoryRequest::None;
};

static_assert(sizeof(LayoutHistory) < 256u * 1024u, "history lives in the context, keep it small");

}
