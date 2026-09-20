#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/ActionId.h"

namespace oryx
{

// Sized above Tic-Tac-Toe's 9 plies; longer games spill to the heap.
constexpr size_t kActionHistoryInlineCapacity = 16;

class ActionHistory
{
public:
    void record(ActionId action);

    [[nodiscard]] bool can_undo() const { return m_cursor > 0; }
    [[nodiscard]] bool can_redo() const { return m_cursor < m_actions.size(); }

    // Both return INVALID_ACTION when there is nothing to undo/redo; the caller still applies the matching IState::undo()/apply().
    ActionId undo();
    ActionId redo();

    [[nodiscard]] std::span<const ActionId> actions() const { return { m_actions.begin(), m_cursor }; }
    [[nodiscard]] size_t size() const { return m_cursor; }
    [[nodiscard]] bool empty() const { return m_cursor == 0; }

private:
    SmallVector<ActionId, kActionHistoryInlineCapacity> m_actions;
    size_t m_cursor = 0;
};

} // namespace oryx
