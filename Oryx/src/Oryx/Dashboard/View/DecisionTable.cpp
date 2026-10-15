#include "oxpch.h"

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

constexpr size_t k_min_history = 2;
constexpr float k_history_height = 100.0f;
constexpr float k_pad = 8.0f;
constexpr float k_marker_width = 3.0f;
constexpr float k_marker_gap = 6.0f;
constexpr float k_column_gap = 10.0f;
constexpr float k_rank_width = 18.0f;
constexpr float k_pill_pad = 4.0f;
constexpr float k_pill_scale = 0.8f;
constexpr float k_number_width = 52.0f;
constexpr float k_number_gap = 4.0f;
constexpr float k_bar_height = 10.0f;
constexpr float k_min_label_width = 44.0f;
constexpr float k_max_label_share = 0.45f;
constexpr float k_chosen_tint = 0.16f;
constexpr float k_rest_alpha = 0.55f;
constexpr float k_zero_tick = 2.0f;
constexpr float k_zero_line_alpha = 0.5f;

[[nodiscard]] Colour with_alpha(const Colour& colour, float alpha)
{
    return { colour.r, colour.g, colour.b, alpha };
}

[[nodiscard]] float sort_key(const ScoreEntry& score, bool by_probability)
{
    if (by_probability)
    {
        return score.has_probability ? static_cast<float>(score.probability) : -std::numeric_limits<float>::infinity();
    }
    return score.has_value ? static_cast<float>(score.value) : -std::numeric_limits<float>::infinity();
}

[[nodiscard]] std::string_view value_text(FrameArena& arena, float value)
{
    return math::abs(value) < 100.0f ? arena.format("%+.2f", static_cast<double>(value)) : arena.format("%+.0f", static_cast<double>(value));
}

struct Columns
{
    bool probabilities = false;
    bool values = false;
};

void draw_track(Painter& painter, const Rect& row, float x, float track_width, const GuiTheme& theme)
{
    painter.fill_rounded_rect({ { x, row.min[1] + (row.size[1] - k_bar_height) * 0.5f }, { track_width, k_bar_height } }, uniform_radius(2.0f), theme.field.background);
}

} // namespace

void draw_decision_table(DashboardModel& model, const RecordView& view, const DecisionColumns& wanted)
{
    const DashboardRecord& record = *view.record;
    Columns shown;
    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        shown.probabilities = shown.probabilities || (wanted.probabilities && view.scores[i].has_probability);
        shown.values = shown.values || (wanted.values && view.scores[i].has_value);
    }
    if (!shown.probabilities && !shown.values)
    {
        gui::label(wanted.probabilities && wanted.values ? "Strategy reports no probabilities or values" : wanted.probabilities ? "Strategy reports no probabilities" : "Strategy reports no values");
        return;
    }

    FrameArena& arena = gui::context().arena();
    const GuiTheme& theme = gui::theme();
    const float row_height = theme.row_height;
    std::span<uint32_t> order = arena.allocate_array<uint32_t>(record.score_count);
    size_t rows = 0;
    float extent = 0.0f;
    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        const ScoreEntry& score = view.scores[i];
        if ((shown.probabilities && score.has_probability) || (shown.values && score.has_value))
        {
            order[rows++] = i;
        }
        if (shown.values && score.has_value)
        {
            extent = math::max(extent, math::abs(static_cast<float>(score.value)));
        }
    }
    if (extent <= math::EPSILON<float>)
    {
        extent = 1.0f;
    }
    // Stable, so equal scores keep the strategy's own order.
    std::stable_sort(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(rows), [&](uint32_t a, uint32_t b) { return sort_key(view.scores[a], shown.probabilities) > sort_key(view.scores[b], shown.probabilities); });

    bool flat_values = shown.values;
    float first_value = 0.0f;
    bool seen_value = false;
    for (size_t row = 0; row < rows && flat_values; ++row)
    {
        const ScoreEntry& score = view.scores[order[row]];
        if (!score.has_value)
        {
            continue;
        }
        flat_values = !seen_value || static_cast<float>(score.value) == first_value;
        first_value = static_cast<float>(score.value);
        seen_value = true;
    }
    flat_values = flat_values && seen_value && rows > 1;

    const float height = static_cast<float>(rows + 1) * row_height;
    const ImId rows_id = make_im_id("decision-rows", gui::context().current_id());
    im::CanvasArea area = gui::canvas("decision-rows", grow(), fixed(height));
    Painter& painter = area.painter;
    const Rect& rect = area.rect;
    const float text_height = theme.base.text_height;

    const float pill_height = text_height * k_pill_scale;
    const float pill_width = painter.text_width("chosen", pill_height) + k_pill_pad * 2.0f;
    float label_width = k_min_label_width;
    for (size_t row = 0; row < rows; ++row)
    {
        const bool chosen_row = view.scores[order[row]].action == record.chosen;
        label_width = math::max(label_width, painter.text_width(label_view(view.scores[order[row]].label), text_height) + (chosen_row ? pill_width + k_pill_pad : 0.0f));
    }
    const float rank_x = rect.min[0] + k_pad + k_marker_width + k_marker_gap;
    const float left = rank_x + k_rank_width;
    const float right = rect.min[0] + rect.size[0] - k_pad;
    label_width = math::min(label_width, math::max(k_min_label_width, (right - left) * k_max_label_share));
    const float columns = static_cast<float>((shown.probabilities ? 1 : 0) + (shown.values ? 1 : 0));
    const float column_width = math::max(0.0f, (right - left - label_width - k_column_gap * columns) / columns);
    const float track_width = math::max(0.0f, column_width - k_number_width - k_number_gap);
    float next_x = left + label_width + k_column_gap;
    const float probability_x = next_x;
    next_x += shown.probabilities ? column_width + k_column_gap : 0.0f;
    const float value_x = next_x;

    const Colour muted = with_alpha(theme.base.text, 0.6f);
    TextSpec header_text;
    header_text.pixel_height = text_height;
    header_text.colour = muted;
    painter.text({ { rank_x, rect.min[1] }, { k_rank_width, row_height } }, "#", header_text);
    painter.text({ { left, rect.min[1] }, { label_width, row_height } }, "Action", header_text);
    if (shown.probabilities)
    {
        painter.text({ { probability_x, rect.min[1] }, { column_width, row_height } }, "Probability", header_text);
    }
    if (shown.values)
    {
        painter.text({ { value_x, rect.min[1] }, { column_width, row_height } }, "Value", header_text);
    }

    if (area.item.hovered && area.pointer_local[1] < row_height)
    {
        const float x = area.pointer_local[0] + rect.min[0];
        if (shown.probabilities && x >= probability_x && x < probability_x + column_width)
        {
            gui::tooltip(area.item, "How likely the strategy was to play this action");
        }
        else if (shown.values && x >= value_x && x < value_x + column_width)
        {
            gui::tooltip(area.item, "Estimated outcome for the player to move: +1 win, 0 even, -1 loss");
        }
    }

    size_t hovered_row = rows;
    if (area.item.hovered && area.pointer_local[1] >= row_height)
    {
        hovered_row = static_cast<size_t>((area.pointer_local[1] - row_height) / row_height);
    }
    const ActionId board_action = model.board_hovered_action;
    model.hovered_action = hovered_row < rows ? view.scores[order[hovered_row]].action : INVALID_ACTION;
    if (area.item.clicked && hovered_row < rows)
    {
        const ActionId clicked = view.scores[order[hovered_row]].action;
        model.pinned_action = model.pinned_action == clicked ? INVALID_ACTION : clicked;
    }

    for (size_t row = 0; row < rows; ++row)
    {
        const ScoreEntry& score = view.scores[order[row]];
        const bool chosen = score.action == record.chosen;
        const Rect line = { { rect.min[0], rect.min[1] + static_cast<float>(row + 1) * row_height }, { rect.size[0], row_height } };
        if (chosen)
        {
            painter.fill_rect(line, with_alpha(theme.base.accent, k_chosen_tint));
            painter.fill_rect({ { line.min[0] + k_pad * 0.5f, line.min[1] + 2.0f }, { k_marker_width, line.size[1] - 4.0f } }, theme.base.accent);
        }
        const bool lit = row == hovered_row || (is_game_action(board_action) && score.action == board_action);
        const Colour row_tint = im::row_fill(gui::context(), make_im_index_id(row, rows_id), theme.base, lit, false, false);
        if (row_tint.a > math::EPSILON<float>)
        {
            painter.fill_rect(line, row_tint);
        }
        if (score.action == model.pinned_action)
        {
            painter.border(line, {}, 1.0f, theme.base.accent);
            painter.icon({ { line.min[0] + line.size[0] - row_height, line.min[1] }, { row_height, row_height } }, Icon::Pin, theme.base.accent, row_height * 0.5f);
        }

        TextSpec label_spec;
        label_spec.pixel_height = text_height;
        label_spec.colour = chosen ? theme.base.text : with_alpha(theme.base.text, 0.85f);
        label_spec.ellipsis = true;
        TextSpec rank_spec;
        rank_spec.pixel_height = text_height;
        rank_spec.colour = muted;
        rank_spec.tabular = true;
        painter.text({ { rank_x, line.min[1] }, { k_rank_width - k_number_gap, row_height } }, arena.format("%zu", row + 1), rank_spec);
        float shown_label_width = label_width;
        if (chosen && painter.text_width(label_view(score.label), text_height) + pill_width + k_pill_pad <= label_width)
        {
            const Rect pill = { { left + label_width - pill_width, line.min[1] + (row_height - pill_height - 4.0f) * 0.5f }, { pill_width, pill_height + 4.0f } };
            painter.fill_rounded_rect(pill, uniform_radius(pill.size[1] * 0.5f), theme.base.accent);
            TextSpec pill_spec;
            pill_spec.pixel_height = pill_height;
            pill_spec.colour = theme.base.on_accent;
            pill_spec.align = TextAlign::Centre;
            painter.text(pill, "chosen", pill_spec);
            shown_label_width = math::max(0.0f, label_width - pill_width - k_pill_pad);
        }
        painter.text({ { left, line.min[1] }, { shown_label_width, row_height } }, label_view(score.label), label_spec);

        TextSpec number_spec;
        number_spec.pixel_height = text_height;
        number_spec.colour = label_spec.colour;
        number_spec.align = TextAlign::Right;
        number_spec.tabular = true;
        if (shown.probabilities && score.has_probability)
        {
            const float probability = math::clamp(static_cast<float>(score.probability), 0.0f, 1.0f);
            draw_track(painter, line, probability_x, track_width, theme);
            painter.fill_rounded_rect({ { probability_x, line.min[1] + (row_height - k_bar_height) * 0.5f }, { track_width * probability, k_bar_height } }, uniform_radius(2.0f), with_alpha(theme.base.accent, chosen ? 1.0f : k_rest_alpha));
            painter.text({ { probability_x + track_width + k_number_gap, line.min[1] }, { k_number_width, row_height } }, arena.format("%.1f%%", static_cast<double>(probability) * 100.0), number_spec);
        }
        if (shown.values && score.has_value)
        {
            const float value = static_cast<float>(score.value);
            draw_track(painter, line, value_x, track_width, theme);
            const float centre = value_x + track_width * 0.5f;
            const float reach = track_width * 0.5f * math::min(1.0f, math::abs(value) / extent);
            const float top = line.min[1] + (row_height - k_bar_height) * 0.5f;
            if (reach >= 1.0f && !flat_values)
            {
                const Colour fill = value > 0.0f ? theme.palette[0] : theme.palette[1];
                painter.fill_rect({ { value > 0.0f ? centre : centre - reach, top }, { reach, k_bar_height } }, with_alpha(fill, chosen ? 1.0f : k_rest_alpha + 0.2f));
            }
            painter.fill_rect({ { centre - k_zero_tick * 0.5f, top - 2.0f }, { k_zero_tick, k_bar_height + 4.0f } }, with_alpha(theme.base.text, k_zero_line_alpha));
            painter.text({ { value_x + track_width + k_number_gap, line.min[1] }, { k_number_width, row_height } }, value_text(arena, value), number_spec);
        }
    }
    gui::caption(flat_values ? "No evaluation: every action has the same value" : "Hover a row to see it on the board, click to pin it");
    if (shown.values && !flat_values)
    {
        gui::caption("Blue: good for the player to move, orange: bad");
    }
}

void draw_value_history(DashboardModel& model)
{
    const DashboardFeed& feed = *model.feed;
    const RecordView selected = selected_view(model);
    if (feed.size() < k_min_history || !selected.valid)
    {
        return;
    }
    gui::caption("Chosen value over the match");
    float first = 0.0f;
    bool varies = false;
    for (size_t index = 0; index < feed.size() && !varies; ++index)
    {
        const float value = feed.view(index).record->chosen_value;
        varies = index > 0 && value != first;
        first = value;
    }
    if (!varies)
    {
        gui::caption("The chosen value has not changed yet");
        return;
    }
    const Values history = { &feed.data()->chosen_value, static_cast<uint32_t>(feed.size()), static_cast<uint32_t>(feed.first_slot()), sizeof(DashboardRecord) };
    gui::PlotScope plot("chosen value", { .height = fixed(k_history_height) });
    plot.line("value", history);
    const uint64_t oldest = feed.view(0).record->sequence;
    plot.vline(static_cast<float>(selected.record->sequence - oldest), gui::theme().base.accent);
    const gui::PlotResult result = plot.result();
    if (result.item.clicked && result.hover_index != gui::k_no_index)
    {
        select_record(model, oldest + result.hover_index);
    }
}

} // namespace oryx
