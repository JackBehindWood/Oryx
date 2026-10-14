#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Dashboard/View/DecisionTable.h"
#include "Oryx/Dashboard/View/ViewFormat.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiWidgets.h"

namespace oryx
{

namespace
{

class DecisionView final : public IDashboardView
{
public:
    [[nodiscard]] std::string_view title() const override { return "Decision"; }

    void draw(DashboardModel& model) override
    {
        const RecordView view = selected_view(model);
        if (!view.valid)
        {
            gui::empty_state("No decisions yet", "Make a move or start a match");
            return;
        }
        FrameArena& arena = gui::context().arena();
        const DashboardRecord& record = *view.record;
        const std::string_view player = player_name(arena, record.player);
        gui::text_coloured(arena.format("%.*s chose %.*s", static_cast<int32_t>(player.size()), player.data(), static_cast<int32_t>(record.chosen_label.length), record.chosen_label.text), player_colour(gui::theme(), record.player));
        gui::caption(arena.format("Decision %u of match %u", record.decision_index + 1, record.match_index + 1));
        draw_decision_table(model, view, {});
        draw_value_history(model);
    }
};

OX_REGISTER_DASHBOARD_VIEW(DecisionView, "decision")

} // namespace

} // namespace oryx
