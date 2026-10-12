#pragma once

#include "Oryx/Dashboard/Feed/DashboardModel.h"

namespace oryx
{

struct DecisionColumns
{
    bool probabilities = true;
    bool values = true;
};

// Action | probability bar | value bar for one record, best first, the chosen row accented; a column the strategy never filled is left out. Draws on the active GuiContext and sets
// model.hovered_action (row under the pointer) and pinned_action (row clicked; clicking it again lets go). Says why when there is nothing to show.
void draw_decision_table(DashboardModel& model, const RecordView& view, const DecisionColumns& columns);

// The chosen value over the ring with the selected decision marked; click selects that decision. Drawn from two records on, since one is only a line.
void draw_value_history(DashboardModel& model);

} // namespace oryx
