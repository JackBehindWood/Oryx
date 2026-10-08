#pragma once

#include "Oryx/Interface/GUI/Dock/DockLayout.h"

// Pure geometry for a DockLayout: no context, no drawing, input is a surface rect and a pointer. Assumes a layout that passed validate.
namespace oryx::gui
{

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
    uint8_t first_visible = 0;
    uint8_t visible_count = 0;
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
};

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

}
