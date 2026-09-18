#include "doctest.h"

#include "unit/Game/DummyGame.h"

using namespace oryx;
using namespace oryx::test;

TEST_CASE("SimulationLayer drives a headless batch of Matches through Application::run() and stops once complete")
{
    Application app({ 0, nullptr });

    UniquePtr<IGame> game = create_unique<DummyGame>(10);
    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(create_unique<DummyGreedyStrategy>());
    strategies.push_back(create_unique<DummyGreedyStrategy>());

    SimulationLayer& layer = app.push_layer<SimulationLayer>(std::move(game), std::move(strategies), /*match_count=*/3);

    app.run();

    CHECK(layer.result().matches == 3);
}

TEST_CASE("SimulationLayer stops cleanly when an ExternalStrategy seat's input closes mid-match")
{
    Application app({ 0, nullptr });

    UniquePtr<IGame> game = create_unique<DummyGame>(10);

    // Only one legal-but-scripted move available; the seat "closes" (like
    // stdin running out) on the next call, before any match finishes.
    bool used = false;
    UniquePtr<IStrategy> external = create_unique<ExternalStrategy>(
        [&](const Context&) -> ActionId
        {
            if (used)
            {
                return INVALID_ACTION;
            }
            used = true;
            return 1;
        });

    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(create_unique<DummyGreedyStrategy>());
    strategies.push_back(std::move(external));

    SimulationLayer& layer = app.push_layer<SimulationLayer>(std::move(game), std::move(strategies), /*match_count=*/5);

    app.run();

    CHECK(layer.result().matches == 0);
}
