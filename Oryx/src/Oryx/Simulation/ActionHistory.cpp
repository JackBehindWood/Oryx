#include "ActionHistory.h"

namespace oryx
{

void ActionHistory::record(ActionId action)
{
    m_actions.push_back(action);
    m_redo_stack.clear();
}

bool ActionHistory::can_undo() const
{
    return !m_actions.empty();
}

bool ActionHistory::can_redo() const
{
    return !m_redo_stack.empty();
}

ActionId ActionHistory::undo()
{
    ActionId action = m_actions.back();
    m_actions.pop_back();
    m_redo_stack.push_back(action);
    return action;
}

ActionId ActionHistory::redo()
{
    ActionId action = m_redo_stack.back();
    m_redo_stack.pop_back();
    m_actions.push_back(action);
    return action;
}

} // namespace oryx
