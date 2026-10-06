#include "TraceRecorder.h"

namespace oryx
{

void TraceRecorder::on_decision(const IState& state, const Decision& decision)
{
    TraceEntry entry;
    entry.ply = static_cast<int32_t>(m_entries.size());
    entry.decision = decision;
    entry.chosen_label = is_game_action(decision.chosen) ? state.action_to_string(decision.chosen) : std::string();
    entry.score_labels.reserve(decision.scores.size());
    for (const ActionScore& score : decision.scores)
    {
        entry.score_labels.push_back(state.action_to_string(score.action));
    }
    m_entries.push_back(std::move(entry));
}

} // namespace oryx
