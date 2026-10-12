#include "DecisionAnnotations.h"

namespace oryx
{

namespace
{

constexpr float k_unset = std::numeric_limits<float>::quiet_NaN();

BoardAnnotation make_item(AnnotationKind kind, const uint32_t* spaces, uint8_t count)
{
    BoardAnnotation item;
    item.kind = kind;
    item.path_count = static_cast<uint8_t>(math::min<size_t>(count, k_max_annotation_path));
    std::copy(spaces, spaces + item.path_count, item.path);
    item.source = k_dashboard_annotation_source;
    return item;
}

SpaceId last_space(const uint32_t* spaces, uint8_t count)
{
    return count == 0 ? k_no_space : spaces[count - 1];
}

} // namespace

void annotate_decision(const RecordView& view, BoardAnnotations& out)
{
    clear_source(out, k_dashboard_annotation_source);
    if (!view.valid)
    {
        return;
    }
    const DashboardRecord& record = *view.record;
    if (record.chosen_space_count > 0)
    {
        BoardAnnotation chosen = make_item(AnnotationKind::Chosen, record.chosen_spaces, record.chosen_space_count);
        chosen.weight = 1.0f;
        chosen.value = record.chosen_value;
        add_annotation(out, chosen);
    }

    bool by_probability = false;
    float low = std::numeric_limits<float>::infinity();
    float high = -std::numeric_limits<float>::infinity();
    float extent = 0.0f;
    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        const ScoreEntry& score = view.scores[i];
        by_probability = by_probability || (score.has_probability && score.space_count > 0);
        if (score.has_value && score.space_count > 0)
        {
            low = math::min(low, static_cast<float>(score.value));
            high = math::max(high, static_cast<float>(score.value));
            extent = math::max(extent, math::abs(static_cast<float>(score.value)));
        }
    }
    if (extent <= math::EPSILON<float>)
    {
        extent = 1.0f;
    }

    float strongest = 0.0f;
    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        const ScoreEntry& score = view.scores[i];
        const SpaceId end = last_space(score.spaces, score.space_count);
        if (end == k_no_space)
        {
            continue;
        }
        float total = 0.0f;
        for (uint32_t j = 0; j < record.score_count; ++j)
        {
            const ScoreEntry& other = view.scores[j];
            if (last_space(other.spaces, other.space_count) == end && other.has_probability)
            {
                total += static_cast<float>(other.probability);
            }
        }
        strongest = math::max(strongest, total);
    }

    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        const ScoreEntry& score = view.scores[i];
        const SpaceId end = last_space(score.spaces, score.space_count);
        if (end == k_no_space)
        {
            continue;
        }
        if (score.has_value)
        {
            BoardAnnotation chip = make_item(AnnotationKind::Value, score.spaces, score.space_count);
            chip.value = static_cast<float>(score.value);
            chip.weight = chip.value / extent;
            add_annotation(out, chip);
        }
        bool first_at_end = true;
        float total = 0.0f;
        float best = -std::numeric_limits<float>::infinity();
        for (uint32_t j = 0; j < record.score_count; ++j)
        {
            const ScoreEntry& other = view.scores[j];
            if (last_space(other.spaces, other.space_count) != end)
            {
                continue;
            }
            first_at_end = first_at_end && j >= i;
            total += other.has_probability ? static_cast<float>(other.probability) : 0.0f;
            best = other.has_value ? math::max(best, static_cast<float>(other.value)) : best;
        }
        if (!first_at_end)
        {
            continue;
        }
        BoardAnnotation heat = make_item(AnnotationKind::Heat, &end, 1);
        if (by_probability && strongest > math::EPSILON<float>)
        {
            heat.weight = total / strongest;
            heat.value = total;
        }
        else if (!by_probability && std::isfinite(best))
        {
            heat.weight = high - low > math::EPSILON<float> ? (best - low) / (high - low) : 1.0f;
            heat.value = k_unset;
        }
        else
        {
            continue;
        }
        add_annotation(out, heat);
    }
}

SpaceId action_destination(const RecordView& view, ActionId action)
{
    if (!view.valid || !is_game_action(action))
    {
        return k_no_space;
    }
    const DashboardRecord& record = *view.record;
    if (action == record.chosen && record.chosen_space_count > 0)
    {
        return last_space(record.chosen_spaces, record.chosen_space_count);
    }
    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        if (view.scores[i].action == action)
        {
            return last_space(view.scores[i].spaces, view.scores[i].space_count);
        }
    }
    return k_no_space;
}

ActionId action_ending_at(const RecordView& view, SpaceId space)
{
    if (!view.valid || space == k_no_space)
    {
        return INVALID_ACTION;
    }
    const DashboardRecord& record = *view.record;
    if (last_space(record.chosen_spaces, record.chosen_space_count) == space)
    {
        return record.chosen;
    }
    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        if (last_space(view.scores[i].spaces, view.scores[i].space_count) == space)
        {
            return view.scores[i].action;
        }
    }
    return INVALID_ACTION;
}

} // namespace oryx
