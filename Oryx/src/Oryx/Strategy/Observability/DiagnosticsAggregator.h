#pragma once

#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

// Folds Decision::extra into Metrics. Keys must be "namespace/name" and may not use a built-in experiment namespace (wins, reward).
// Sums by default; keys ending in "_max" keep the maximum. Each namespace seen in a decision adds one to "<namespace>/decisions".
class DiagnosticsAggregator : public IDecisionObserver
{
public:
    void on_decision(const IState& state, const Decision& decision) override;

    [[nodiscard]] const Metrics& metrics() const { return m_metrics; }
    void clear() { m_metrics.values.clear(); }

private:
    Metrics m_metrics;
};

} // namespace oryx
