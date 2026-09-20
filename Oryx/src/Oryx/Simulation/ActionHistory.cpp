#include "ActionHistory.h"

namespace oryx
{

void ActionHistory::record(ActionId action)
{
    while (m_actions.size() > m_cursor)
    {
        m_actions.pop_back();
    }
    m_actions.push_back(action);
    ++m_cursor;
}

ActionId ActionHistory::undo()
{
    if (!can_undo())
    {
        return INVALID_ACTION;
    }
    --m_cursor;
    return m_actions[m_cursor];
}

ActionId ActionHistory::redo()
{
    if (!can_redo())
    {
        return INVALID_ACTION;
    }
    ActionId action = m_actions[m_cursor];
    ++m_cursor;
    return action;
}

} // namespace oryx
