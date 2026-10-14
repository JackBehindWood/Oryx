#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Dashboard/View/DecisionTable.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiControls.h"
#include "Oryx/Interface/GUI/GuiData.h"

namespace oryx
{

namespace
{

class ProbabilitiesView final : public IDashboardView
{
public:
    [[nodiscard]] std::string_view title() const override { return "Probabilities"; }

    void draw(DashboardModel& model) override
    {
        const RecordView view = selected_view(model);
        if (!view.valid)
        {
            gui::empty_state("No decisions yet", "Make a move or start a match");
            return;
        }
        draw_decision_table(model, view, { true, false });
    }
};

OX_REGISTER_DASHBOARD_VIEW(ProbabilitiesView, "probabilities")

} // namespace

} // namespace oryx
