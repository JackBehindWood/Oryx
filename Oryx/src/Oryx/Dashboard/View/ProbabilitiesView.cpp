#include "oxpch.h"

#include "Oryx/Dashboard/View/DashboardViewRegistry.h"
#include "Oryx/Dashboard/View/DecisionTable.h"
#include "Oryx/Dashboard/View/ViewFormat.h"
#include "Oryx/Interface/GUI/Gui.h"
#include "Oryx/Interface/GUI/GuiControls.h"
#include "Oryx/Interface/GUI/GuiData.h"
#include "Oryx/Interface/GUI/GuiMenus.h"

namespace oryx
{

namespace
{

constexpr float k_strip_height = 20.0f;
constexpr float k_strip_gap = 1.0f;
constexpr float k_min_alpha = 0.35f;

struct Distribution
{
    std::span<uint32_t> order;
    size_t count = 0;
    double total = 0.0;
    double entropy = 0.0;
};

[[nodiscard]] Distribution distribution_of(FrameArena& arena, const RecordView& view)
{
    Distribution result;
    result.order = arena.allocate_array<uint32_t>(view.record->score_count);
    for (uint32_t i = 0; i < view.record->score_count; ++i)
    {
        if (view.scores[i].has_probability && view.scores[i].probability > 0.0)
        {
            result.order[result.count++] = i;
            result.total += view.scores[i].probability;
        }
    }
    std::stable_sort(result.order.begin(), result.order.begin() + static_cast<std::ptrdiff_t>(result.count), [&](uint32_t a, uint32_t b) { return view.scores[a].probability > view.scores[b].probability; });
    for (size_t row = 0; row < result.count && result.total > 0.0; ++row)
    {
        const double p = view.scores[result.order[row]].probability / result.total;
        result.entropy -= p * std::log2(p);
    }
    return result;
}

// One bar the width of the panel, a segment per action in proportion to its probability, strongest first; the chosen one is outlined.
void draw_strip(DashboardModel& model, const RecordView& view, const Distribution& distribution)
{
    const GuiTheme& theme = gui::theme();
    im::CanvasArea area = gui::canvas("probability-strip", grow(), fixed(k_strip_height));
    Painter& painter = area.painter;
    const float count = static_cast<float>(distribution.count);
    float x = area.rect.min[0];
    size_t hovered = distribution.count;
    for (size_t row = 0; row < distribution.count; ++row)
    {
        const ScoreEntry& score = view.scores[distribution.order[row]];
        const float width = area.rect.size[0] * static_cast<float>(score.probability / distribution.total);
        const Rect segment = { { x, area.rect.min[1] }, { math::max(width - k_strip_gap, 1.0f), area.rect.size[1] } };
        if (area.item.hovered && area.pointer_local[0] + area.rect.min[0] >= x && area.pointer_local[0] + area.rect.min[0] < x + width)
        {
            hovered = row;
        }
        const float rank = count > 1.0f ? static_cast<float>(row) / (count - 1.0f) : 0.0f;
        const float alpha = row == hovered ? 1.0f : math::lerp(1.0f, k_min_alpha, rank);
        painter.fill_rounded_rect(segment, uniform_radius(2.0f), { theme.base.accent.r, theme.base.accent.g, theme.base.accent.b, alpha });
        if (score.action == view.record->chosen)
        {
            painter.border(segment, uniform_radius(2.0f), 1.5f, theme.base.text);
        }
        x += width;
    }
    if (hovered < distribution.count)
    {
        const ScoreEntry& score = view.scores[distribution.order[hovered]];
        model.hovered_action = score.action;
        FrameArena& arena = gui::context().arena();
        gui::tooltip(area.item, arena.format("%.*s  %.1f%%", static_cast<int32_t>(score.label.length), score.label.text, score.probability / distribution.total * 100.0));
        if (area.item.clicked)
        {
            model.pinned_action = model.pinned_action == score.action ? INVALID_ACTION : score.action;
        }
    }
}

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
        FrameArena& arena = gui::context().arena();
        const Distribution distribution = distribution_of(arena, view);
        if (distribution.count == 0 || distribution.total <= 0.0)
        {
            draw_decision_table(model, view, { true, false });
            return;
        }
        const ScoreEntry& top = view.scores[distribution.order[0]];
        const std::string_view player = player_name(arena, view.record->player);
        gui::text_coloured(arena.format("Most likely: %.*s \xC2\xB7 %.1f%%", static_cast<int32_t>(top.label.length), top.label.text, top.probability / distribution.total * 100.0), player_colour(gui::theme(), view.record->player));
        const double spread = distribution.count > 1 ? std::log2(static_cast<double>(distribution.count)) : 0.0;
        gui::caption(arena.format("%zu options \xC2\xB7 %.2f of %.2f bits of uncertainty \xC2\xB7 %.*s chose %.*s", distribution.count, distribution.entropy, spread, static_cast<int32_t>(player.size()), player.data(), static_cast<int32_t>(view.record->chosen_label.length), view.record->chosen_label.text));
        draw_strip(model, view, distribution);
        draw_decision_table(model, view, { true, false });
    }
};

OX_REGISTER_DASHBOARD_VIEW(ProbabilitiesView, "probabilities")

} // namespace

} // namespace oryx
