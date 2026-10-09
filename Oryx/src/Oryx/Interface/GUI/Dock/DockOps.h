#pragma once

#include "Oryx/Interface/GUI/Dock/DockLayout.h"

// Pure operations on a DockLayout. They assume a layout that passed validate (the host validates on load), never throw, and on a refusal leave the layout untouched.
namespace oryx::gui
{

enum class DockReason : uint8_t
{
    None,
    UnknownPanel,
    NotPermittedReorder,
    NotPermittedDock,
    NotPermittedFloat,
    NotPermittedResize,
    NotPermittedCollapse,
    NotPermittedClose,
    NoChange,
    TargetInvalid,
    BadArgument,
    NotFound,
    TabsFull,
    NodesFull,
    FloatsFull,
    PoolFull,
    AlreadyOpen,
    AlreadyClosed,
    NotPermittedTarget,
    Collapsed
};

[[nodiscard]] const char* to_string(DockReason reason);
// The refusal as a short sentence for the person dragging; to_string stays the developer wording for logs and tests.
[[nodiscard]] const char* describe(DockReason reason);

// applied is true exactly when reason is None.
struct DockResult
{
    bool applied = false;
    DockReason reason = DockReason::None;
};

// One permission set for ops, cursor and preview feedback, and tests: None means allowed.
[[nodiscard]] DockReason can_reorder(const PanelTable& panels, PanelId panel);
[[nodiscard]] DockReason can_dock(const PanelTable& panels, PanelId panel);
[[nodiscard]] DockReason can_float(const PanelTable& panels, PanelId panel);
[[nodiscard]] DockReason can_resize(const PanelTable& panels, PanelId panel);
[[nodiscard]] DockReason can_collapse(const PanelTable& panels, PanelId panel);
[[nodiscard]] DockReason can_close(const PanelTable& panels, PanelId panel);

// can_dock plus the panel's dock_only / dock_never rules against the Tabs node `target` (k_dock_root: only allowed without dock_only). Same set as dock_panel, for drop feedback.
[[nodiscard]] DockReason can_dock_into(const DockLayout& layout, const PanelTable& panels, PanelId panel, int32_t target);

// True when the panel sits in a Tabs node or a float.
[[nodiscard]] bool is_open(const DockLayout& layout, PanelId panel);
// True when every Tabs node under `node` is collapsed.
[[nodiscard]] bool all_collapsed(const DockLayout& layout, int32_t node);
// True when set_split would not refuse: a resize-capable panel sits on each side and, in a vertical split, neither side is collapsed (it is one strip tall).
[[nodiscard]] bool can_resize_split(const DockLayout& layout, const PanelTable& panels, int32_t node);

// Centre tabs into the target Tabs node; an edge splits it with the panel on that side. target is a Tabs node, or dock_root(surface) for that surface's tree (an edge there wraps the whole tree).
[[nodiscard]] DockResult dock_panel(DockLayout& layout, const PanelTable& panels, PanelId panel, DockTarget target, DropZone zone);
[[nodiscard]] DockResult float_panel(DockLayout& layout, const PanelTable& panels, PanelId panel, const Rect& rect, uint8_t surface = 0);
[[nodiscard]] DockResult close_panel(DockLayout& layout, const PanelTable& panels, PanelId panel);
// Reopens at the recorded home when its sibling is open, else by the panel's placement hints (see PanelDesc), else tabs into the first Tabs node of `surface`.
[[nodiscard]] DockResult open_panel(DockLayout& layout, const PanelTable& panels, PanelId panel, uint8_t surface = 0);
// Fills `split` as the node holding `leaf` and `target` on `zone`'s axis, the leaf on the zone's side. `points` > 0 fixes the leaf's size in points (FixedFirst/FixedSecond), else the leaf takes k_dock_new_ratio.
void init_split(DockNode& split, DropZone zone, int32_t leaf, int32_t target, float points = 0.0f);
// True when any panel registered in `group` is open.
[[nodiscard]] bool group_open(const DockLayout& layout, const PanelTable& panels, PanelId group);
// Opens (each at its home or hinted place) or closes every panel of the group; NoChange when none moved, NotFound when the group has no panel.
[[nodiscard]] DockResult set_group_open(DockLayout& layout, const PanelTable& panels, PanelId group, bool open);
// The default layout from the registered panels: every panel with initial_open, in (order, registration) order, placed by its hints on an empty layout.
[[nodiscard]] DockLayout build_default_layout(const PanelTable& panels);
[[nodiscard]] DockResult reorder_tab(DockLayout& layout, const PanelTable& panels, PanelId panel, uint32_t index);
[[nodiscard]] DockResult select_tab(DockLayout& layout, int32_t node, uint32_t index);
// Expanding is never refused; collapsing needs every tab to allow it and leaves at least one Tabs node of the surface expanded (Collapsed).
[[nodiscard]] DockResult set_collapsed(DockLayout& layout, const PanelTable& panels, int32_t node, bool collapsed);
// Ratio is clamped to [k_dock_min_ratio, 1 - k_dock_min_ratio] and points to >= 0; minimum panel sizes are the solver's job.
[[nodiscard]] DockResult set_split(DockLayout& layout, const PanelTable& panels, int32_t node, DockSizeMode mode, float ratio, float points);

// Drops empty tabs and splits missing a child, prunes stale closed entries and homes, clamps fields and rewrites nodes in depth-first order so equal layouts compare equal.
void normalize(DockLayout& layout);
// Also expands any node holding a panel that may not collapse.
void normalize(DockLayout& layout, const PanelTable& panels);

// Moves floats and trees of surfaces >= surface_count onto surface 0 (a saved layout can outlive the window it was made in); panels of a dropped tree become tabs of surface 0's first stack.
void migrate_surfaces(DockLayout& layout, const PanelTable& panels, uint32_t surface_count);

struct ValidateFlags
{
    bool require_viewport = false;
};

// Throws oryx::Error on corrupt data: bad counts, indices, cycles, shared nodes, duplicate panels, non-finite numbers; with a table also a collapsed node holding a panel that may not collapse.
void validate(const DockLayout& layout);
void validate(const DockLayout& layout, const PanelTable& panels, ValidateFlags flags);

// Field-wise over the used entries (padding and unused slots are ignored).
[[nodiscard]] bool equal(const DockLayout& a, const DockLayout& b);

// Deterministic indented text; names come from the table when given, else ids print as #hex.
[[nodiscard]] std::string dump(const DockLayout& layout);
[[nodiscard]] std::string dump(const DockLayout& layout, const PanelTable& panels);
// Empty when the dumps match, else -/+ lines for each differing line.
[[nodiscard]] std::string diff(const DockLayout& a, const DockLayout& b);

inline constexpr uint32_t k_dock_history_size = 32;

// A ring of layout snapshots for undo/redo: cursor is the current entry, redo lies after it.
struct LayoutHistory
{
    DockLayout snapshots[k_dock_history_size] = {};
    uint32_t oldest = 0;
    uint32_t count = 0;
    uint32_t cursor = 0;
};

void reset(LayoutHistory& history, const DockLayout& layout);
// No-op when equal to the current entry; drops redo entries and, when full, the oldest.
void push(LayoutHistory& history, const DockLayout& layout);
[[nodiscard]] bool can_undo(const LayoutHistory& history);
[[nodiscard]] bool can_redo(const LayoutHistory& history);
[[nodiscard]] bool undo(LayoutHistory& history, DockLayout& out);
[[nodiscard]] bool redo(LayoutHistory& history, DockLayout& out);

}
