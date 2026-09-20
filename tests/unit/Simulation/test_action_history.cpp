#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

namespace
{

std::vector<ActionId> applied(const ActionHistory& history)
{
    return std::vector<ActionId>(history.actions().begin(), history.actions().end());
}

} // namespace

TEST_CASE("ActionHistory records actions in order")
{
    ActionHistory history;
    CHECK(history.empty());

    history.record(1);
    history.record(2);
    history.record(3);

    CHECK(history.size() == 3);
    CHECK(applied(history) == std::vector<ActionId>{ 1, 2, 3 });
}

TEST_CASE("ActionHistory::undo pops the last action and enables redo")
{
    ActionHistory history;
    history.record(1);
    history.record(2);

    CHECK(history.can_undo());
    CHECK_FALSE(history.can_redo());

    ActionId undone = history.undo();

    CHECK(undone == 2);
    CHECK(applied(history) == std::vector<ActionId>{ 1 });
    CHECK(history.can_redo());
}

TEST_CASE("ActionHistory::redo re-applies the most recently undone action")
{
    ActionHistory history;
    history.record(1);
    history.record(2);
    history.undo();

    ActionId redone = history.redo();

    CHECK(redone == 2);
    CHECK(applied(history) == std::vector<ActionId>{ 1, 2 });
    CHECK_FALSE(history.can_redo());
}

TEST_CASE("ActionHistory::record after an undo discards the redo stack")
{
    ActionHistory history;
    history.record(1);
    history.record(2);
    history.undo();

    CHECK(history.can_redo());

    history.record(3);

    CHECK_FALSE(history.can_redo());
    CHECK(applied(history) == std::vector<ActionId>{ 1, 3 });
}

TEST_CASE("ActionHistory::can_undo is false on an empty history")
{
    ActionHistory history;
    CHECK_FALSE(history.can_undo());
}

TEST_CASE("ActionHistory::undo and redo return INVALID_ACTION instead of touching an empty history")
{
    ActionHistory history;

    CHECK(history.undo() == INVALID_ACTION);
    CHECK(history.redo() == INVALID_ACTION);
    CHECK(history.empty());

    history.record(1);
    CHECK(history.redo() == INVALID_ACTION);
    CHECK(history.undo() == 1);
    CHECK(history.undo() == INVALID_ACTION);
    CHECK(history.redo() == 1);
}

TEST_CASE("ActionHistory keeps recording past its inline capacity")
{
    ActionHistory history;
    for (ActionId action = 0; action < 2 * kActionHistoryInlineCapacity; ++action)
    {
        history.record(action);
    }

    CHECK(history.size() == 2 * kActionHistoryInlineCapacity);
    CHECK(history.actions().front() == 0);
    CHECK(history.actions().back() == 2 * kActionHistoryInlineCapacity - 1);
}
