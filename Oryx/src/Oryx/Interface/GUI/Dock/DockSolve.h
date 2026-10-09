#pragma once

#include "Oryx/Interface/GUI/Dock/DockLayout.h"
#include "Oryx/Interface/GUI/Dock/DockOps.h"

// Pure geometry for a DockLayout: no context, no drawing, input is a surface rect and a pointer. Assumes a layout that passed validate.
namespace oryx::gui
{

enum class TabPosition : uint8_t
{
    Top,
    Bottom
};

enum class CloseButtons : uint8_t
{
    OnHover,
    Always,
    Never
};

// The user's look-and-feel choices, all plain values so a settings loader fills them. Precedence for toolbars: this style, then the panel's own placement, then the node's context rule.
struct DockStyle
{
    ToolbarPlacement toolbar_placement = ToolbarPlacement::Auto;
    bool toolbars = true;
    TabPosition tab_position = TabPosition::Top;
    CloseButtons close_buttons = CloseButtons::Always;
    // Scales strip and toolbar heights and tab padding.
    bool compact = false;
    // Alpha factor of the drop preview fill.
    float preview_opacity = 1.0f;
    // The compass of drop guides shown while a panel is dragged; the edge bands still work without it.
    bool guides = true;
    // A short accent flash on the panel that just docked, so the landing spot is unmistakable.
    bool drop_flash = true;
};

// Points; the host scales them by the theme scale.
struct DockMetrics
{
    float strip_height = 22.0f;
    float splitter = 4.0f;
    float tab_min_width = 48.0f;
    float tab_max_width = 160.0f;
    // Fraction of a Tabs body, from each edge, that drops as a split instead of a tab.
    float edge_band = 0.25f;
    // Distance in points from the surface edge that docks beside the whole tree.
    float root_edge = 12.0f;
    // Width reserved at the right end of a strip for the collapse chevron when every tab may collapse; zero reserves nothing.
    float strip_button = 22.0f;
    float toolbar_height = 24.0f;
    // Side of one drop guide, the gap between guides of a compass and the gap between the outer guides and the surface edge.
    float guide_size = 28.0f;
    float guide_gap = 4.0f;
    float guide_inset = 6.0f;
    DockStyle style;
};

struct SolvedNode
{
    Rect rect;
    // Tabs nodes only: the strip, the body below it (empty when collapsed) and the visible window of tabs.
    Rect strip;
    Rect body;
    Rect tab_rects[k_max_dock_tabs];
    // Tabs nodes: the collapse chevron's box, empty when collapsing is not permitted. Split nodes: the gap between the two children.
    Rect collapse_button;
    Rect splitter;
    // Tabs nodes: the selected panel's toolbar strip, empty when it has none (or the node is collapsed).
    Rect toolbar;
    uint8_t first_visible = 0;
    uint8_t visible_count = 0;
};

// A float's parts: a title bar of strip_height, then the toolbar when the panel has one, then the body.
struct SolvedFloat
{
    Rect title;
    Rect toolbar;
    Rect body;
};

struct SolvedLayout
{
    uint8_t surface = 0;
    Rect surface_rect;
    DockMetrics metrics;
    uint32_t node_count = 0;
    uint32_t float_count = 0;
    SolvedNode nodes[k_max_dock_nodes];
    // Parallel to DockLayout::floats: clamped inside the surface, zero for floats of other surfaces.
    Rect floats[k_max_dock_floats];
    SolvedFloat float_parts[k_max_dock_floats];
};

// The placement a node's selected panel gets after style, panel and context are weighed.
[[nodiscard]] ToolbarPlacement resolve_toolbar_placement(const DockStyle& style, const PanelDesc& panel, uint32_t tab_count);

// Tabs that do not fit keep tab_min_width and scroll so the selected one is visible; a surface smaller than the panels' minimums shrinks both sides proportionally and bodies clip.
[[nodiscard]] SolvedLayout solve(const DockLayout& layout, const PanelTable& panels, const DockMetrics& metrics, const Rect& surface_rect, uint8_t surface = 0);

// Where a dragged panel would land. Combine with can_dock; floats are ignored. The preview is the ratio rect of the new split; panel minimums can make the solved rect larger.
struct DropTarget
{
    bool valid = false;
    int32_t node = k_no_node;
    DropZone zone = DropZone::Centre;
    Rect preview;
};

[[nodiscard]] DropTarget drop_target(const DockLayout& layout, const SolvedLayout& solved, const Vec2f& pointer);

inline constexpr uint32_t k_max_drop_guides = 9;

// One target of the compass: a zone of a Tabs node (or, with node k_dock_root, beside the whole tree). Refused guides stay present, flagged.
struct DropGuide
{
    DropZone zone = DropZone::Centre;
    int32_t node = k_no_node;
    Rect rect;
    bool allowed = false;
    DockReason reason = DockReason::None;
};

struct DropGuides
{
    DropGuide guides[k_max_drop_guides];
    uint32_t count = 0;
};

// The compass for dragging `panel` with the pointer at `pointer`: centre plus four edge guides around the hovered node's body, and four outer guides at the surface edges. A node whose body is too small for
// the cluster keeps the centre guide only, then nothing; the outer guides need room for themselves. Empty when the style turns guides off, the pointer is outside the surface or the panel may not dock at all.
// Permission is can_dock_into, the same set dock_panel uses; a floating panel gets no centre-body guide, matching the float rule of resolve_drop.
[[nodiscard]] DropGuides drop_guides(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer);
// Index of the guide under the pointer, or -1.
[[nodiscard]] int32_t guide_at(const DropGuides& guides, const Vec2f& pointer);
// The inner mark of a guide's icon: the half of the box the zone would take, the middle for Centre.
[[nodiscard]] Rect guide_glyph(const Rect& guide, DropZone zone);

enum class DropAction : uint8_t
{
    // Nothing to apply and nothing to preview (the pointer is where the drag began, or a float is only being moved).
    None,
    Reorder,
    Dock,
    Float,
    Cancel
};

// What releasing a dragged panel at the pointer would do; a pointer over a guide wins over the bands. `slot` is the tab position for Reorder and for a Centre dock; reason says why a Cancel was refused.
struct DropPlan
{
    DropAction action = DropAction::None;
    int32_t node = k_no_node;
    DropZone zone = DropZone::Centre;
    uint32_t slot = 0;
    Rect preview;
    // The insertion bar in the target strip for Reorder and a Centre dock; empty otherwise.
    Rect marker;
    DockReason reason = DockReason::None;
    // The guide under the pointer that decided the plan, or -1 when a band did.
    int32_t guide = -1;
};

inline constexpr float k_dock_float_width = 320.0f;
inline constexpr float k_dock_float_height = 240.0f;

// The one place that decides a drag, shared by the host's preview and its release. `float_only` (Shift) skips every dock target; a pointer outside the surface also floats when the panel may.
[[nodiscard]] DropPlan resolve_drop(const DockLayout& layout, const PanelTable& panels, const SolvedLayout& solved, PanelId panel, const Vec2f& pointer, bool float_only);

}
