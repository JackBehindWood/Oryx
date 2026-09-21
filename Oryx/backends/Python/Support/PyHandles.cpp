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

} // namespace oryx::python
