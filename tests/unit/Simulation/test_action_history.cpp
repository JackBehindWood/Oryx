#include "doctest.h"

#include "Oryx.h"

using namespace oryx;

TEST_CASE("ActionHistory records actions in order")
{
    ActionHistory history;
    CHECK(history.empty());

    history.record(1);
    history.record(2);
    history.record(3);

    CHECK(history.size() == 3);
    CHECK(history.actions() == std::vector<ActionId>{ 1, 2, 3 });
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
    CHECK(history.actions() == std::vector<ActionId>{ 1 });
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
    CHECK(history.actions() == std::vector<ActionId>{ 1, 2 });
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
    CHECK(history.actions() == std::vector<ActionId>{ 1, 3 });
}

TEST_CASE("ActionHistory::can_undo is false on an empty history")
{
    ActionHistory history;
    CHECK_FALSE(history.can_undo());
}
