#include "DashboardFeed.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Metrics.h"

namespace oryx
{

namespace
{

void validate(const FeedOptions& options)
{
    if (options.capacity < 1 || options.max_scores < 1 || options.max_metrics < 1 || options.max_tree_nodes < 0)
    {
        throw Error("FeedOptions: capacity, max_scores and max_metrics must be at least 1 and max_tree_nodes at least 0");
    }
    if (options.max_keys < 1 || options.max_keys > k_max_feed_keys)
    {
        throw Error("FeedOptions: max_keys must be between 1 and " + std::to_string(k_max_feed_keys));
    }
}

} // namespace

DashboardFeed::DashboardFeed(const FeedOptions& options)
{
    reset(options);
}

void DashboardFeed::reset(const FeedOptions& options)
{
    validate(options);
    m_options = options;
    size_t capacity = static_cast<size_t>(options.capacity);
    m_records.assign(capacity, DashboardRecord{});
    m_scores.assign(capacity * static_cast<size_t>(options.max_scores), ScoreEntry{});
    m_metrics.assign(capacity * static_cast<size_t>(options.max_metrics), MetricEntry{});
    m_tree.assign(capacity * static_cast<size_t>(options.max_tree_nodes), SearchNode{});
    m_key_names = std::vector<std::string>();
    m_key_names.reserve(static_cast<size_t>(options.max_keys));
    m_key_stats.assign(static_cast<size_t>(options.max_keys), KeyStats{});
    clear();
}

void DashboardFeed::clear()
{
    // Sequences keep counting so a selection made before a clear can never alias a later record.
    m_base_sequence += m_total;
    m_total = 0;
    m_replay_sourced = false;
    m_match_index = 0;
    m_decision_index = 0;
    m_drops = FeedDrops{};
    m_key_names.clear();
    std::fill(m_key_stats.begin(), m_key_stats.end(), KeyStats{});
}

void DashboardFeed::begin_match()
{
    if (m_replay_sourced)
    {
        return;
    }
    if (m_clear_at_next_match)
    {
        m_clear_at_next_match = false;
        clear();
        return;
    }
    if (m_decision_index > 0)
    {
        ++m_match_index;
    }
    m_decision_index = 0;
}

int32_t DashboardFeed::find_key(std::string_view name) const
{
    for (size_t i = 0; i < m_key_names.size(); ++i)
    {
        if (m_key_names[i] == name)
        {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

int32_t DashboardFeed::intern_key(const std::string& name)
{
    int32_t found = find_key(name);
    if (found >= 0)
    {
        return found;
    }
    if (m_key_names.size() >= static_cast<size_t>(m_options.max_keys))
    {
        return -1;
    }
    m_key_names.push_back(name);
    m_key_stats[m_key_names.size() - 1].keeps_max = is_max_metric(name);
    return static_cast<int32_t>(m_key_names.size() - 1);
}

size_t DashboardFeed::store_numbers(const Decision& decision)
{
    size_t slot = static_cast<size_t>(m_total % m_records.size());
    DashboardRecord& record = m_records[slot];
    record = DashboardRecord{};
    record.sequence = m_base_sequence + m_total;
    record.match_index = m_match_index;
    record.decision_index = m_decision_index++;
    record.player = decision.player;
    record.chosen = decision.chosen;

    // Scanned before truncation so the chosen action's numbers survive a small max_scores.
    for (const ActionScore& score : decision.scores)
    {
        if (score.action == decision.chosen)
        {
            if (score.has_value)
            {
                record.chosen_value = static_cast<float>(score.value);
            }
            if (score.has_probability)
            {
                record.chosen_probability = static_cast<float>(score.probability);
            }
            break;
        }
    }

    size_t score_limit = static_cast<size_t>(m_options.max_scores);
    ScoreEntry* scores = m_scores.data() + slot * score_limit;
    size_t kept_scores = decision.scores.size() < score_limit ? decision.scores.size() : score_limit;
    for (size_t i = 0; i < kept_scores; ++i)
    {
        const ActionScore& score = decision.scores[i];
        scores[i] = ScoreEntry{};
        scores[i].action = score.action;
        scores[i].probability = score.probability;
        scores[i].value = score.value;
        scores[i].has_probability = score.has_probability;
        scores[i].has_value = score.has_value;
    }
    record.score_count = static_cast<uint32_t>(kept_scores);
    record.dropped_scores = static_cast<uint32_t>(decision.scores.size() - kept_scores);

    size_t metric_limit = static_cast<size_t>(m_options.max_metrics);
    MetricEntry* metrics = m_metrics.data() + slot * metric_limit;
    for (const auto& [name, value] : decision.extra.values)
    {
        int32_t key = intern_key(name);
        if (key < 0)
        {
            ++record.dropped_metrics;
            continue;
        }
        KeyStats& stats = m_key_stats[static_cast<size_t>(key)];
        stats.sum += value;
        stats.max = stats.count == 0 ? value : math::max(stats.max, value);
        stats.last = value;
        ++stats.count;
        if (record.metric_count < metric_limit)
        {
            metrics[record.metric_count++] = MetricEntry{ static_cast<uint16_t>(key), value };
        }
        else
        {
            ++record.dropped_metrics;
        }
    }

    size_t tree_limit = static_cast<size_t>(m_options.max_tree_nodes);
    size_t kept_nodes = decision.tree.size() < tree_limit ? decision.tree.size() : tree_limit;
    if (kept_nodes > 0)
    {
        std::copy(decision.tree.begin(), decision.tree.begin() + static_cast<std::ptrdiff_t>(kept_nodes), m_tree.begin() + static_cast<std::ptrdiff_t>(slot * tree_limit));
    }
    record.tree_count = static_cast<uint32_t>(kept_nodes);
    record.dropped_tree_nodes = static_cast<uint32_t>(decision.tree.size() - kept_nodes);

    m_drops.scores += record.dropped_scores;
    m_drops.metrics += record.dropped_metrics;
    m_drops.tree_nodes += decision.tree.size() - kept_nodes;
    ++m_total;
    return slot;
}

void DashboardFeed::describe_into(const IState& state, ActionId action, FixedLabel& label, uint32_t* spaces, uint8_t& space_count) const
{
    ActionDescription description;
    if (m_describer != nullptr && m_describer->describe(state, action, description))
    {
        label = description.label;
        space_count = static_cast<uint8_t>(math::min<size_t>(description.space_count, k_record_spaces));
        std::copy(description.spaces, description.spaces + space_count, spaces);
        return;
    }
    set_label(label, state.action_to_string(action));
    space_count = 0;
}

void DashboardFeed::store_labels(size_t slot, const IState& state, const Decision& decision)
{
    DashboardRecord& record = m_records[slot];
    if (is_game_action(decision.chosen))
    {
        describe_into(state, decision.chosen, record.chosen_label, record.chosen_spaces, record.chosen_space_count);
    }
    ScoreEntry* scores = m_scores.data() + slot * static_cast<size_t>(m_options.max_scores);
    for (uint32_t i = 0; i < record.score_count; ++i)
    {
        describe_into(state, scores[i].action, scores[i].label, scores[i].spaces, scores[i].space_count);
    }
}

void DashboardFeed::on_decision(const IState& state, const Decision& decision)
{
    if (m_replay_sourced)
    {
        return;
    }
    size_t slot = store_numbers(decision);
    store_labels(slot, state, decision);
}

void DashboardFeed::adopt_keys(const ReplayLog& log)
{
    size_t count = math::min(log.key_count(), static_cast<size_t>(m_options.max_keys));
    m_key_names.clear();
    std::fill(m_key_stats.begin(), m_key_stats.end(), KeyStats{});
    for (size_t i = 0; i < count; ++i)
    {
        m_key_names.emplace_back(log.key_name(static_cast<uint16_t>(i)));
        m_key_stats[i] = log.key_stats(static_cast<uint16_t>(i));
    }
}

void DashboardFeed::load_window(ReplayLog& log, uint64_t first)
{
    uint64_t capacity = m_records.size();
    uint64_t total = log.record_count();
    uint64_t count = math::min(total, capacity);
    first = total > capacity ? math::min(first, total - capacity) : 0;

    uint64_t keep_begin = 0;
    uint64_t keep_end = 0;
    if (m_replay_sourced)
    {
        keep_begin = m_total - size();
        keep_end = m_total;
    }
    else
    {
        m_base_sequence = 0;
        m_total = 0;
    }

    size_t score_limit = static_cast<size_t>(m_options.max_scores);
    size_t metric_limit = static_cast<size_t>(m_options.max_metrics);
    size_t tree_limit = static_cast<size_t>(m_options.max_tree_nodes);
    for (uint64_t sequence = first; sequence < first + count; ++sequence)
    {
        if (sequence >= keep_begin && sequence < keep_end)
        {
            continue;
        }
        size_t slot = static_cast<size_t>(sequence % capacity);
        ParsedRecord parsed;
        parsed.scores = m_scores.data() + slot * score_limit;
        parsed.score_capacity = static_cast<uint32_t>(score_limit);
        parsed.metrics = m_metrics.data() + slot * metric_limit;
        parsed.metric_capacity = static_cast<uint32_t>(metric_limit);
        parsed.tree = m_tree.data() + slot * tree_limit;
        parsed.tree_capacity = static_cast<uint32_t>(tree_limit);
        if (!log.read(sequence, parsed))
        {
            parsed.record = DashboardRecord{};
        }
        DashboardRecord& record = parsed.record;
        uint32_t kept = 0;
        for (uint32_t i = 0; i < record.metric_count; ++i)
        {
            if (parsed.metrics[i].key < m_key_names.size())
            {
                parsed.metrics[kept++] = parsed.metrics[i];
            }
        }
        record.dropped_metrics += record.metric_count - kept;
        record.metric_count = kept;
        record.sequence = sequence;
        m_records[slot] = record;
    }

    m_replay_sourced = true;
    m_base_sequence = 0;
    m_total = first + count;
    m_clear_at_next_match = false;
    m_drops = FeedDrops{};
    for (size_t i = 0; i < size(); ++i)
    {
        const DashboardRecord& record = *view(i).record;
        m_drops.scores += record.dropped_scores;
        m_drops.metrics += record.dropped_metrics;
        m_drops.tree_nodes += record.dropped_tree_nodes;
    }
}

RecordView DashboardFeed::view_of_slot(size_t slot) const
{
    RecordView result;
    result.record = &m_records[slot];
    result.scores = m_scores.data() + slot * static_cast<size_t>(m_options.max_scores);
    result.metrics = m_metrics.data() + slot * static_cast<size_t>(m_options.max_metrics);
    result.tree = m_tree.data() + slot * static_cast<size_t>(m_options.max_tree_nodes);
    result.valid = true;
    return result;
}

RecordView DashboardFeed::view(size_t index) const
{
    if (index >= size())
    {
        return RecordView{};
    }
    return view_of_slot((first_slot() + index) % m_records.size());
}

RecordView DashboardFeed::find_sequence(uint64_t sequence) const
{
    uint64_t oldest = m_base_sequence + m_total - size();
    if (sequence < oldest || sequence >= m_base_sequence + m_total)
    {
        return RecordView{};
    }
    return view(static_cast<size_t>(sequence - oldest));
}

} // namespace oryx
