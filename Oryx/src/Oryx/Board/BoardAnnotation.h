#pragma once

#include "Oryx/Board/Layout/BoardLayout.h"

namespace oryx
{

enum class AnnotationKind : uint8_t
{
    // A tint under the pieces of the path's last space; `weight` (0..1, relative) sets its strength and a finite `value` prints as a percentage.
    Heat,
    // A ring over the last space and, for a path of several spaces, an arrow along it.
    Chosen,
    // A signed chip on the last space while it is hovered; `value` is the number, its sign the colour.
    Value
};

constexpr size_t k_max_annotation_path = 8;

// What a producer wants said about the board, in spaces only, so a 2D renderer now and a 3D one later read the same list. `source` tags the producer (the dashboard, a hint engine, a threat map) so each can replace its own items.
struct BoardAnnotation
{
    AnnotationKind kind = AnnotationKind::Heat;
    uint32_t path[k_max_annotation_path] = {};
    uint8_t path_count = 0;
    float weight = 0.0f;
    float value = std::numeric_limits<float>::quiet_NaN();
    uint8_t source = 0;
};

struct BoardAnnotations
{
    std::vector<BoardAnnotation> items;
};

// Which kinds a viewer wants drawn; independent switches, not modes.
struct AnnotationFlags
{
    bool heat = false;
    bool chosen = false;
    bool value = false;
};

[[nodiscard]] constexpr bool operator==(const AnnotationFlags& a, const AnnotationFlags& b)
{
    return a.heat == b.heat && a.chosen == b.chosen && a.value == b.value;
}

[[nodiscard]] constexpr bool is_on(const AnnotationFlags& flags, AnnotationKind kind)
{
    return kind == AnnotationKind::Heat ? flags.heat : kind == AnnotationKind::Chosen ? flags.chosen : flags.value;
}

[[nodiscard]] constexpr bool any_on(const AnnotationFlags& flags)
{
    return flags.heat || flags.chosen || flags.value;
}

// Drops every item of `source`; the capacity stays so refilling a warm list allocates nothing.
inline void clear_source(BoardAnnotations& annotations, uint8_t source)
{
    std::erase_if(annotations.items, [source](const BoardAnnotation& item) { return item.source == source; });
}

inline void add_annotation(BoardAnnotations& annotations, const BoardAnnotation& item)
{
    annotations.items.push_back(item);
}

// The last space of the path, k_no_space for an empty one.
[[nodiscard]] inline SpaceId destination(const BoardAnnotation& item)
{
    return item.path_count == 0 ? k_no_space : item.path[item.path_count - 1];
}

} // namespace oryx
