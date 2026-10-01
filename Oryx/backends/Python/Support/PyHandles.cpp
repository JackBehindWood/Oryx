#include "oxpch.h"
#include "Support/PyHandles.h"

#include "Oryx/Scripting/Support/ScriptUtil.h"

namespace oryx::python
{

PyState::PyState(UniquePtr<IState> owned)
    : m_owned(std::move(owned))
    , m_state(m_owned.get())
{
}

PyState::PyState(IState& borrowed, SharedPtr<ScriptLease> lease, StateAccess access)
    : m_state(&borrowed)
    , m_lease(std::move(lease))
    , m_access(access)
{
}

IState& PyState::get() const
{
    if (m_lease)
    {
        m_lease->require("state");
    }
    return *m_state;
}

std::vector<ActionId> PyState::legal_actions() const
{
    ActionList actions = get().legal_actions();
    return std::vector<ActionId>(actions.begin(), actions.end());
}

void PyState::apply(ActionId action) const
{
    require_writable();
    IState& state = get();
    check_legal(state, action);
    state.apply(action);
}

void PyState::undo(ActionId action) const
{
    require_writable();
    get().undo(action);
}

void PyState::require_writable() const
{
    if (m_access == StateAccess::ReadOnly)
    {
        throw Error("this state belongs to a match and is read-only: use match.apply() and match.undo()");
    }
}

std::vector<int32_t> PyActionFeatures::decode(ActionId action) const
{
    m_lease->require("action features");
    SmallVector<int32_t, 2> decoded = m_features.decode(action);
    return std::vector<int32_t>(decoded.begin(), decoded.end());
}

std::vector<double> PyState::outcome() const
{
    return rewards_to_vector(get().outcome().rewards);
}

PyContext::PyContext(const Context& context, SharedPtr<ScriptLease> lease)
    : m_state(create_shared<PyState>(context.state(), lease))
    , m_lease(std::move(lease))
{
    if (const IActionFeatures* features = context.get<IActionFeatures>())
    {
        m_features = create_shared<PyActionFeatures>(*features, m_lease);
    }
    if (IDecisionObserver* observer = context.get<IDecisionObserver>())
    {
        m_observer = create_shared<PyObserver>(*observer, context.state(), m_lease);
    }
}

void PyObserver::publish(ActionId chosen, const std::map<ActionId, double>& probabilities, const std::map<ActionId, double>& values, const std::map<std::string, double>& extra) const
{
    m_lease->require("observer");
    check_legal(m_state, chosen, "observer.publish() chosen action");

    Decision decision;
    decision.player = m_state.current_player();
    decision.chosen = chosen;
    for (const auto& [action, probability] : probabilities)
    {
        check_legal(m_state, action, "observer.publish() probabilities action");
        set_probability(decision, action, probability);
    }
    for (const auto& [action, value] : values)
    {
        check_legal(m_state, action, "observer.publish() values action");
        set_value(decision, action, value);
    }
    decision.extra.values = extra;
    m_observer.on_decision(m_state, decision);
}

SharedPtr<PyState> PyContext::state() const
{
    m_lease->require("context");
    return m_state;
}

SharedPtr<PyActionFeatures> PyContext::action_features() const
{
    m_lease->require("context");
    return m_features;
}

SharedPtr<PyObserver> PyContext::observer() const
{
    m_lease->require("context");
    return m_observer;
}

} // namespace oryx::python
