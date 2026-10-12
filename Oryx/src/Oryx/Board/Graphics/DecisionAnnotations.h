#pragma once

#include "Oryx/Board/BoardAnnotation.h"
#include "Oryx/Dashboard/Feed/DashboardRecord.h"

namespace oryx
{

// The source tag of what the dashboard says about a board, so another producer can stack its own items.
constexpr uint8_t k_dashboard_annotation_source = 1;

// Replaces the dashboard's items in `out` with the record's decision: the chosen move, a heat item per destination space (probability summed over the actions that end there, or the best value for a value-only strategy,
// relative to the strongest) and a value item per scored action. Actions without spaces are skipped. An invalid view leaves only the removal.
void annotate_decision(const RecordView& view, BoardAnnotations& out);

// The space an action ends on, k_no_space for an unknown action or one without spaces.
[[nodiscard]] SpaceId action_destination(const RecordView& view, ActionId action);

// The action to show for a pointer over `space`: the chosen one when it ends there, else the first scored action that does; INVALID_ACTION when none.
[[nodiscard]] ActionId action_ending_at(const RecordView& view, SpaceId space);

} // namespace oryx
