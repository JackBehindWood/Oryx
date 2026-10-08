#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiData.h"

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
            gui::label("No decision to show");
            return;
        }
        draw_action_values(view);
        draw_history(model);
    }

private:
    // Each record is scaled by its own largest magnitude, with zero at mid-bar, so minimax, win-rate and centipawn strategies read alike.
    static void draw_action_values(const RecordView& view)
    {
        const DashboardRecord& record = *view.record;
        FrameArena& arena = gui::context().arena();
        float extent = 0.0f;
        for (uint32_t i = 0; i < record.score_count; ++i)
        {
            if (view.scores[i].has_value)
            {
                extent = math::max(extent, math::abs(static_cast<float>(view.scores[i].value)));
            }
        }
        if (extent <= math::EPSILON<float>)
        {
            extent = 1.0f;
        }
        const ColourScale scale = diverging_scale(0.0f, 2.0f * extent);
        bool any = false;
        for (uint32_t i = 0; i < record.score_count; ++i)
        {
            const ScoreEntry& score = view.scores[i];
            if (!score.has_value)
            {
                continue;
            }
            any = true;
            gui::push_id(arena.format("%u", i));
            const float value = static_cast<float>(score.value);
            gui::BarOptions options;
            options.scale = &scale;
            options.value_text = arena.format("%.2f", static_cast<double>(value));
            gui::bar(label_view(score.label), value + extent, 2.0f * extent, options);
            gui::pop_id();
        }
        if (!any)
        {
            gui::label("Strategy reports no values");
        }
    }

    static void draw_history(DashboardModel& model)
    {
        const DashboardFeed& feed = *model.feed;
        const Values history = { &feed.data()->chosen_value, static_cast<uint32_t>(feed.size()), static_cast<uint32_t>(feed.first_slot()), sizeof(DashboardRecord) };
        gui::PlotScope plot("chosen value", { .height = fixed(100.0f) });
        plot.line("value", history);
        const RecordView selected = selected_view(model);
        const uint64_t oldest = feed.view(0).record->sequence;
        plot.vline(static_cast<float>(selected.record->sequence - oldest), gui::theme().base.accent);
        const gui::PlotResult result = plot.result();
        if (result.item.clicked && result.hover_index != gui::k_no_index)
        {
            select_record(model, oldest + result.hover_index);
        }
    }
};

OX_REGISTER_DASHBOARD_VIEW(ValuesView, "values")

} // namespace

} // namespace oryx
