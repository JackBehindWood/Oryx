#include "doctest.h"

#include "../Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("ExternalStrategy::decide forwards to the injected callback")
{
    DummyGame game(10);
    auto state = game.new_initial_state();
    Context context(*state);

    UniquePtr<ExternalStrategy> strategy = make_external([](const Context&) -> ActionId { return 3; });

    CHECK(strategy->decide(context) == 3);
}

TEST_CASE("ExternalStrategy passes UNDO_ACTION/INVALID_ACTION through unchanged")
{
    DummyGame game(10);
    auto state = game.new_initial_state();
    Context context(*state);

    UniquePtr<ExternalStrategy> undo_strategy = make_external([](const Context&) -> ActionId { return UNDO_ACTION; });
    CHECK(undo_strategy->decide(context) == UNDO_ACTION);

    UniquePtr<ExternalStrategy> closed_strategy = make_external([](const Context&) -> ActionId { return INVALID_ACTION; });
    CHECK(closed_strategy->decide(context) == INVALID_ACTION);
}

TEST_CASE("ExternalStrategy::required_capabilities defaults to empty, like any IStrategy")
{
    UniquePtr<ExternalStrategy> strategy = make_external([](const Context&) -> ActionId { return 0; });
    CHECK(strategy->required_capabilities().empty());
}
