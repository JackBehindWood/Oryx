#pragma once

#include "Oryx.h"

namespace oasis
{

// Forwards every decision to a target and, while enabled, gives a decision that has no search tree a generated one so the dashboard's Search view can be tried before any strategy publishes a tree.
class SyntheticTreeObserver final : public oryx::IDecisionObserver
{
public:
    explicit SyntheticTreeObserver(oryx::IDecisionObserver& target)
        : m_target(target)
    {
    }

    void set_enabled(bool enabled) { m_enabled = enabled; }
    [[nodiscard]] bool enabled() const { return m_enabled; }

    void on_decision(const oryx::IState& state, const oryx::Decision& decision) override;
    void on_match_start() override { m_target.on_match_start(); }

private:
    oryx::IDecisionObserver& m_target;
    oryx::Decision m_decorated;
    uint64_t m_count = 0;
    bool m_enabled = false;
};

} // namespace oasis
