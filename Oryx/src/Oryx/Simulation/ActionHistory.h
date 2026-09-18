#pragma once

#include "Oryx/Game/ActionId.h"

#include <vector>

namespace oryx
{

class ActionHistory
{
public:
    void record(ActionId action);

    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;

    // Caller still applies the corresponding IState::undo()/apply().
    ActionId undo();
    ActionId redo();

    [[nodiscard]] const std::vector<ActionId>& actions() const { return m_actions; }
    [[nodiscard]] size_t size() const { return m_actions.size(); }
    [[nodiscard]] bool empty() const { return m_actions.empty(); }

private:
    std::vector<ActionId> m_actions;
    std::vector<ActionId> m_redo_stack;
};

} // namespace oryx
