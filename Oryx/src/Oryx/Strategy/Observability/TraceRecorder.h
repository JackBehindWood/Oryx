#pragma once

#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

struct TraceEntry
{
    int32_t ply = 0;
    Decision decision;
    std::string chosen_label;
    std::vector<std::string> score_labels;
};

// Copies every decision, resolving action labels while the state is still at the decision position.
class TraceRecorder : public IDecisionObserver
{
public:
    void on_decision(const IState& state, const Decision& decision) override;

    [[nodiscard]] const std::vector<TraceEntry>& entries() const { return m_entries; }
    void clear() { m_entries.clear(); }

private:
    std::vector<TraceEntry> m_entries;
};

} // namespace oryx
