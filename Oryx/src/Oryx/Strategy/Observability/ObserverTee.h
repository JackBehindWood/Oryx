#pragma once

#include "Oryx/Strategy/Observability/IDecisionObserver.h"

namespace oryx
{

// Forwards every call to each target in the order added; targets must outlive the tee.
class ObserverTee final : public IDecisionObserver
{
public:
    void add(IDecisionObserver& target) { m_targets.push_back(&target); }

    void on_decision(const IState& state, const Decision& decision) override
    {
        for (IDecisionObserver* target : m_targets)
        {
            target->on_decision(state, decision);
        }
    }

    void on_match_start() override
    {
        for (IDecisionObserver* target : m_targets)
        {
            target->on_match_start();
        }
    }

    void on_match_end(const Outcome& outcome) override
    {
        for (IDecisionObserver* target : m_targets)
        {
            target->on_match_end(outcome);
        }
    }

private:
    std::vector<IDecisionObserver*> m_targets;
};

} // namespace oryx
