#pragma once

#include "Oryx/Interface/Canvas/Rect.h"

// The dock layout as plain data: flat pools, fixed capacity, no pointers, so it is copied, diffed and saved by value. Behaviour lives in DockOps.h and DockSolve.h.
namespace oryx::gui
{

inline constexpr uint32_t k_dock_version = 1;
inline constexpr uint32_t k_max_dock_nodes = 64;
inline constexpr uint32_t k_max_dock_tabs = 8;
inline constexpr uint32_t k_max_dock_floats = 8;
inline constexpr uint32_t k_max_dock_homes = 32;
inline constexpr uint32_t k_max_dock_closed = 32;
inline constexpr uint32_t k_max_dock_surfaces = 2;
inline constexpr uint32_t k_max_panels = 32;
inline constexpr uint32_t k_panel_text_size = 48;
inline constexpr uint32_t k_max_dock_rules = 4;

inline constexpr int32_t k_no_node = -1;
// A drop target node meaning "the whole tree of the target's surface" rather than one node.
inline constexpr int32_t k_dock_root = -1;

// Where an operation lands: a Tabs node, or k_dock_root for the whole tree, on a surface. A bare node index means surface 0.
struct DockTarget
{
    int32_t node = k_dock_root;
    uint8_t surface = 0;

    constexpr DockTarget() = default;
    constexpr DockTarget(int32_t target_node, uint8_t target_surface = 0) : node(target_node), surface(target_surface) {}
};

[[nodiscard]] constexpr DockTarget dock_root(uint8_t surface = 0) { return DockTarget{ k_dock_root, surface }; }

inline constexpr float k_dock_new_ratio = 0.3f;
inline constexpr float k_dock_min_ratio = 0.05f;

struct PanelId
{
    uint32_t hash = 0;
};

[[nodiscard]] constexpr bool operator==(PanelId a, PanelId b) { return a.hash == b.hash; }
[[nodiscard]] constexpr bool operator!=(PanelId a, PanelId b) { return a.hash != b.hash; }
[[nodiscard]] constexpr bool is_valid(PanelId id) { return id.hash != 0; }

// 32-bit FNV-1a; saved layouts key on these hashes, so the constants and byte order are fixed (golden test).
inline constexpr uint32_t k_panel_fnv_offset = 0x811c9dc5u;
inline constexpr uint32_t k_panel_fnv_prime = 0x01000193u;

// Never returns the invalid id (0).
[[nodiscard]] constexpr PanelId make_panel_id(std::string_view name)
{
    uint32_t hash = k_panel_fnv_offset;
    for (char c : name)
    {
        hash ^= static_cast<uint8_t>(c);
        hash *= k_panel_fnv_prime;
    }
    return PanelId{ hash == 0 ? 1u : hash };
}

enum class PanelKind : uint8_t
{
    View,
    // The host reserves a rect and draws nothing in it.
    Viewport
};

namespace panel_flag
{
inline constexpr uint8_t reorder_in_host = 1u << 0;
inline constexpr uint8_t dock_elsewhere = 1u << 1;
inline constexpr uint8_t tear_off = 1u << 2;
inline constexpr uint8_t resize = 1u << 3;
inline constexpr uint8_t collapse = 1u << 4;
inline constexpr uint8_t close = 1u << 5;
inline constexpr uint8_t all = 0x3Fu;
}

// Where a panel's toolbar strip sits against the tab strip of its node; Auto lets the node decide (a lone panel puts it above the tabs, tabbed panels below).
enum class ToolbarPlacement : uint8_t
{
    Auto,
    AboveTabs,
    BelowTabs
};

// Independent permission bits, not a mode: a pinned panel clears dock_elsewhere and tear_off yet still reorders and resizes.
struct PanelFlags
{
    uint8_t bits = 0;
};

[[nodiscard]] constexpr bool operator==(PanelFlags a, PanelFlags b) { return a.bits == b.bits; }
[[nodiscard]] constexpr bool has_flag(PanelFlags flags, uint8_t bit) { return (flags.bits & bit) == bit; }
[[nodiscard]] constexpr PanelFlags pinned(PanelFlags flags) { return PanelFlags{ static_cast<uint8_t>(flags.bits & ~(panel_flag::dock_elsewhere | panel_flag::tear_off)) }; }

[[nodiscard]] constexpr PanelFlags default_flags(PanelKind kind)
{
    return kind == PanelKind::Viewport ? PanelFlags{ panel_flag::reorder_in_host | panel_flag::dock_elsewhere | panel_flag::resize } : PanelFlags{ panel_flag::all };
}

enum class DropZone : uint8_t
{
    Centre,
    Left,
    Right,
    Top,
    Bottom
};

// Registered by code every run, never saved; names live here so a PanelId stays four bytes.
struct PanelDesc
{
    PanelId id;
    PanelKind kind = PanelKind::View;
    PanelFlags flags{ panel_flag::all };
    float min_w = 0.0f;
    float min_h = 0.0f;
    char name[k_panel_text_size] = {};
    char title[k_panel_text_size] = {};
    // Optional docking limits: when dock_only is not empty the panel may only dock into (tab with or split beside) a node holding one of those panels; it never docks into a node holding a dock_never panel.
    PanelId dock_only[k_max_dock_rules] = {};
    PanelId dock_never[k_max_dock_rules] = {};
    uint8_t dock_only_count = 0;
    uint8_t dock_never_count = 0;
    // Reserves a toolbar strip of DockMetrics::toolbar_height for the panel.
    bool toolbar = false;
    ToolbarPlacement toolbar_placement = ToolbarPlacement::Auto;
    // The body is drawn by another context beneath the host, so the host paints no backdrop there.
    bool foreign_body = false;
    // Placement hints from the registering consumer (ids are hashes of names): the GUI decides and skips what it cannot honour. Chain: recorded home, tabbed_with, near on dock_side, the surface edge, the first tab stack.
    PanelId dock_near;
    PanelId dock_tabbed_with;
    PanelId group;
    DropZone dock_side = DropZone::Right;
    // Points of the new split's fixed side; zero keeps the ratio.
    float dock_size = 0.0f;
    int32_t order = 0;
    // False leaves the panel out of the default layout.
    bool initial_open = true;
};

enum class DockRuleKind : uint8_t
{
    // The panels it may tab with are those of the listed groups (and viewports when asked); anything else is refused.
    OnlyWithGroups,
    // The panels of the listed groups (and viewports when asked) are refused as tab mates.
    NeverWithGroups
};

// One rule of a group: which panels its members may share a tab stack with. Plain data, so presets are values and tests build them directly.
struct DockRule
{
    DockRuleKind kind = DockRuleKind::OnlyWithGroups;
    PanelId groups[k_max_dock_rules] = {};
    uint8_t count = 0;
    // Panels of PanelKind::Viewport count as listed.
    bool viewports = false;
};

// All of a group's rules, every one of which must allow a tab mate.
struct DockRules
{
    DockRule rules[k_max_dock_rules] = {};
    uint8_t count = 0;
};

inline constexpr uint32_t k_max_rule_groups = 8;

struct GroupRules
{
    PanelId group;
    DockRules rules;
};

struct PanelTable
{
    PanelDesc descs[k_max_panels] = {};
    uint32_t count = 0;
    // Registered by code every run like the panels, never saved.
    GroupRules group_rules[k_max_rule_groups] = {};
    uint32_t group_rule_count = 0;
};

namespace detail
{
inline void copy_panel_text(char (&dst)[k_panel_text_size], std::string_view src)
{
    const uint32_t length = src.size() < k_panel_text_size - 1 ? static_cast<uint32_t>(src.size()) : k_panel_text_size - 1;
    for (uint32_t i = 0; i < length; ++i)
        dst[i] = src[i];
    dst[length] = '\0';
}
}

// False when the table is full or the id is already registered; the desc's flags default from its kind.
[[nodiscard]] inline bool add_panel(PanelTable& table, std::string_view name, std::string_view title, PanelKind kind, float min_w = 0.0f, float min_h = 0.0f)
{
    const PanelId id = make_panel_id(name);
    if (table.count >= k_max_panels)
        return false;
    for (uint32_t i = 0; i < table.count; ++i)
        if (table.descs[i].id == id)
            return false;
    PanelDesc& desc = table.descs[table.count++];
    desc = PanelDesc{};
    desc.id = id;
    desc.kind = kind;
    desc.flags = default_flags(kind);
    desc.min_w = min_w;
    desc.min_h = min_h;
    detail::copy_panel_text(desc.name, name);
    detail::copy_panel_text(desc.title, title);
    return true;
}

[[nodiscard]] inline const PanelDesc* find_panel(const PanelTable& table, PanelId id)
{
    for (uint32_t i = 0; i < table.count && i < k_max_panels; ++i)
        if (table.descs[i].id == id)
            return &table.descs[i];
    return nullptr;
}

[[nodiscard]] inline PanelDesc* find_panel(PanelTable& table, PanelId id)
{
    for (uint32_t i = 0; i < table.count && i < k_max_panels; ++i)
        if (table.descs[i].id == id)
            return &table.descs[i];
    return nullptr;
}

namespace detail
{
inline bool add_dock_rule(PanelId (&list)[k_max_dock_rules], uint8_t& count, PanelId other)
{
    if (!is_valid(other) || count >= k_max_dock_rules)
        return false;
    for (uint32_t i = 0; i < count; ++i)
        if (list[i] == other)
            return false;
    list[count++] = other;
    return true;
}
}

// False when the panel is not registered, the list is full or the entry is already there.
[[nodiscard]] inline bool add_dock_only(PanelTable& table, PanelId panel, PanelId target)
{
    PanelDesc* desc = find_panel(table, panel);
    return desc != nullptr && detail::add_dock_rule(desc->dock_only, desc->dock_only_count, target);
}

[[nodiscard]] inline bool add_dock_never(PanelTable& table, PanelId panel, PanelId target)
{
    PanelDesc* desc = find_panel(table, panel);
    return desc != nullptr && detail::add_dock_rule(desc->dock_never, desc->dock_never_count, target);
}

// Sets the rules of `group`, replacing earlier ones; false when `group` is invalid, a rule is malformed (count over capacity) or k_max_rule_groups groups already have rules.
[[nodiscard]] inline bool set_group_rules(PanelTable& table, PanelId group, const DockRules& rules)
{
    if (!is_valid(group) || rules.count > k_max_dock_rules)
        return false;
    for (uint32_t r = 0; r < rules.count; ++r)
        if (rules.rules[r].count > k_max_dock_rules)
            return false;
    for (uint32_t i = 0; i < table.group_rule_count; ++i)
        if (table.group_rules[i].group == group)
        {
            table.group_rules[i].rules = rules;
            return true;
        }
    if (table.group_rule_count >= k_max_rule_groups)
        return false;
    table.group_rules[table.group_rule_count++] = GroupRules{ group, rules };
    return true;
}

[[nodiscard]] inline const DockRules* find_group_rules(const PanelTable& table, PanelId group)
{
    if (!is_valid(group))
        return nullptr;
    for (uint32_t i = 0; i < table.group_rule_count && i < k_max_rule_groups; ++i)
        if (table.group_rules[i].group == group)
            return &table.group_rules[i].rules;
    return nullptr;
}

enum class DockNodeKind : uint8_t
{
    Empty,
    Split,
    Tabs
};

// Horizontal puts `first` left of `second`; Vertical puts `first` above it.
enum class DockAxis : uint8_t
{
    Horizontal,
    Vertical
};

enum class DockSizeMode : uint8_t
{
    Ratio,
    FixedFirst,
    FixedSecond
};

// One struct for every kind (no union) so unused fields stay comparable and a byte-mutating fuzz test only ever sees valid objects.
struct DockNode
{
    DockNodeKind kind = DockNodeKind::Empty;
    DockAxis axis = DockAxis::Horizontal;
    DockSizeMode mode = DockSizeMode::Ratio;
    uint8_t count = 0;
    uint8_t selected = 0;
    uint8_t collapsed = 0;
    float ratio = 0.5f;
    float points = 0.0f;
    int32_t first = k_no_node;
    int32_t second = k_no_node;
    PanelId tabs[k_max_dock_tabs] = {};
};

// An in-window floating panel; tear_off decides whether a panel may be one.
struct DockFloat
{
    PanelId panel;
    uint8_t surface = 0;
    Rect rect;
};

// Where a closed panel was, so reopening puts it back next to the same sibling.
struct DockHome
{
    PanelId panel;
    PanelId sibling;
    DropZone zone = DropZone::Centre;
};

struct DockLayout
{
    uint32_t version = k_dock_version;
    int32_t roots[k_max_dock_surfaces] = { k_no_node, k_no_node };
    uint32_t node_count = 0;
    uint32_t float_count = 0;
    uint32_t home_count = 0;
    uint32_t closed_count = 0;
    DockNode nodes[k_max_dock_nodes] = {};
    DockFloat floats[k_max_dock_floats] = {};
    DockHome homes[k_max_dock_homes] = {};
    PanelId closed[k_max_dock_closed] = {};
};

static_assert(std::is_trivially_copyable_v<PanelId>);
static_assert(std::is_trivially_copyable_v<PanelDesc>);
static_assert(std::is_trivially_copyable_v<DockNode>);
static_assert(std::is_trivially_copyable_v<DockLayout>);

}
