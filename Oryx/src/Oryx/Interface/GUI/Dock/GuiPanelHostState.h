#pragma once

#include "Oryx/Interface/Canvas/ImWidgets.h"
#include "Oryx/Interface/GUI/Dock/DockAutosave.h"
#include "Oryx/Interface/GUI/Dock/DockOps.h"
#include "Oryx/Interface/GUI/Dock/DockSolve.h"

// The data behind the panel host. DockModel is what the user edits and saves (panels, layout, history, drag, focus, autosave) and is shared between the contexts of one app; DockView is one context's frame state
// (rects, solve, bodies). Behaviour lives in GuiPanelHost.h; GuiContext owns one DockModel (or borrows another's) and one DockView.
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
    // The body belongs to another context drawn beneath the host (see panel_rect): no backdrop.
    bool foreign_body = false;
    // Placement hints (see PanelDesc): names of other panels and of a group; the GUI builds the default layout and reopens from them.
    std::string_view dock_near;
    std::string_view dock_tabbed_with;
    std::string_view group;
    DropZone dock_side = DropZone::Right;
    float dock_size = 0.0f;
    int32_t order = 0;
    bool initial_open = true;
};

static_assert(std::is_trivially_copyable_v<PanelOptions>);

// What the last host frame did to the layout, for the owner to push history on release or debounce a save.
struct PanelHostResult
{
    bool layout_changed = false;
    // A splitter is held.
    bool interacting = false;
    // A tab or float is being dragged.
    bool dragging = false;
    // The panel a drop placed this frame, else invalid.
    PanelId landed;
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
    // A tab drag reorders its strip live; where the tab started, so Esc or a refused drop can put it back.
    uint32_t origin_slot = 0;
    bool reordered = false;
};

enum class HistoryRequest : uint8_t
{
    None,
    Undo,
    Redo
};

// Sizes captured when a splitter or float grip drag starts, so every frame applies the press-relative total instead of a clamped per-frame delta.
struct DragAnchor
{
    float size = 0.0f;
    Rect rect;
};

enum class DockLoad : uint8_t
{
    None,
    // Built from hints or set_default_layout; no file was read (docking off, or no layout_file).
    DefaultOnly,
    File
};

struct DockModel
{
    DockModel() = default;
    // Writes a pending layout when this model owns the file.
    ~DockModel();
    DockModel(const DockModel&) = delete;
    DockModel& operator=(const DockModel&) = delete;

    PanelTable panels;
    DockLayout layout;
    DockLayout default_layout;
    bool default_set = false;
    DockLayout last_good;
    bool has_good = false;
    PanelId focused;
    uint32_t reported[k_max_reported_panels] = {};
    uint32_t reported_count = 0;
    DragState drag;
    DragAnchor anchor;
    LayoutHistory history;
    bool history_seeded = false;
    HistoryRequest history_request = HistoryRequest::None;
    // The panel that last landed from a drop and the seconds since, for the flash.
    PanelId landed;
    float landed_age = 0.0f;
    DockLoad load = DockLoad::None;
    bool from_file = false;
    // Surfaces that exist; a saved float or tree beyond it migrates to surface 0 at load.
    uint32_t surface_count = 1;
    // The layout file as read at load, so the final flush needs no settings.
    std::filesystem::path file;
    UniquePtr<DockAutosave> autosave;
};

struct DockView
{
    DockModel* model = nullptr;
    bool in_host = false;
    uint64_t frame = 0;
    bool dock_body_open = false;
    DockLayout* layout = nullptr;
    bool require_viewport = false;
    DockStyle style;
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
    // Last frame's floats back to front, for panel_occluded.
    Rect float_rects[k_max_dock_floats];
    PanelId float_panels[k_max_dock_floats];
    uint32_t float_count = 0;
};

static_assert(sizeof(LayoutHistory) < 256u * 1024u, "history lives in the context, keep it small");

}
