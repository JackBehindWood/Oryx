#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Dashboard/View/DecisionTable.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiControls.h"

namespace oryx
{

namespace
{

class ValuesView final : public IDashboardView
{
public:
    [[nodiscard]] std::string_view title() const override { return "Values"; }

    void draw(DashboardModel& model) override
    {
        const RecordView view = selected_view(model);
        if (!view.valid)
        {
            gui::empty_state("No decisions yet", "Make a move or start a match");
            return;
        }
        draw_decision_table(model, view, { false, true });
        draw_value_history(model);
    }
};

OX_REGISTER_DASHBOARD_VIEW(ValuesView, "values")

} // namespace

} // namespace oryx
