#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Interface/GUI/Gui.h"
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
            gui::label("No decision to show");
            return;
        }
        FrameArena& arena = gui::context().arena();
        const DashboardRecord& record = *view.record;
        gui::label(arena.format("Decision %u, player %u, chose %.*s", record.decision_index, static_cast<uint32_t>(record.player), static_cast<int>(record.chosen_label.length), record.chosen_label.text));

        ImStyle muted = gui::theme().field;
        muted.accent = muted.selected;
        bool any = false;
        gui::ScrollScope scroll("probabilities");
        for (uint32_t i = 0; i < record.score_count; ++i)
        {
            const ScoreEntry& score = view.scores[i];
            if (!score.has_probability)
            {
                continue;
            }
            any = true;
            gui::push_id(arena.format("%u", i));
            gui::BarOptions options;
            options.format = { NumberStyle::Percent, 1 };
            options.style = score.action == record.chosen ? nullptr : &muted;
            gui::bar(label_view(score.label), static_cast<float>(score.probability), 1.0f, options);
            gui::pop_id();
        }
        if (!any)
        {
            gui::label("Strategy reports no probabilities");
        }
    }
};

OX_REGISTER_DASHBOARD_VIEW(ProbabilitiesView, "probabilities")

} // namespace

} // namespace oryx
