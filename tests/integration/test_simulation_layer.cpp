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

    SimulationLayer& layer = app.push_layer<SimulationLayer>();
    StartSimulationEvent start(std::move(game), std::move(strategies), /*match_count=*/3, /*on_turn=*/nullptr, /*benchmark=*/false);
    app.post_event(start);

    app.run();

    CHECK(layer.result().matches == 3);
}

TEST_CASE("SimulationLayer closes with an error when a strategy returns INVALID_ACTION mid-match")
{
    Application app({ 0, nullptr });

    UniquePtr<IGame> game = create_unique<DummyGame>(10);

    // Only one legal-but-scripted move available; the seat "closes" (like
    // stdin running out) on the next call, before any match finishes.
    bool used = false;
    UniquePtr<IStrategy> external = make_external(
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

    SimulationLayer& layer = app.push_layer<SimulationLayer>();
    StartSimulationEvent start(std::move(game), std::move(strategies), /*match_count=*/5, /*on_turn=*/nullptr, /*benchmark=*/false);
    app.post_event(start);

    app.run();

    CHECK(layer.result().matches == 0);
    CHECK(app.exit_code() == 1);
}

TEST_CASE("SimulationLayer retries a PENDING_ACTION decision on later updates instead of closing")
{
    Application app({ 0, nullptr });

    int32_t polls = 0;
    UniquePtr<IStrategy> waiting = make_external(
        [&](const Context&) -> ActionId
        {
            ++polls;
            return polls < 4 ? PENDING_ACTION : 1;
        });

    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(std::move(waiting));
    strategies.push_back(create_unique<DummyGreedyStrategy>());

    SimulationLayer& layer = app.push_layer<SimulationLayer>();
    StartSimulationEvent start(create_unique<DummyGame>(10), std::move(strategies), /*match_count=*/1, /*on_turn=*/nullptr, /*benchmark=*/false);
    app.post_event(start);

    app.run();

    CHECK(polls >= 4);
    CHECK(layer.result().matches == 1);
}

namespace
{

class CountingObserver : public ITurnObserver
{
public:
    void on_turn(IState& state) override
    {
        ++turns;
        if (state.is_terminal())
        {
            ++finished_frames;
        }
    }

    int32_t turns = 0;
    int32_t finished_frames = 0;
};

class RestartingObserver : public ITurnObserver
{
public:
    void on_turn(IState& state) override
    {
        if (!state.is_terminal())
        {
            return;
        }
        ++finished_frames;
        if (finished_frames == 3)
        {
            Application::Get().close();
            return;
        }
        if (finished_frames == 2)
        {
            RestartSimulationEvent restart;
            Application::Get().post_event(restart);
        }
    }

    int32_t finished_frames = 0;
};

} // namespace

TEST_CASE("SimulationLayer calls its observer every update, including on the finished state")
{
    Application app({ 0, nullptr });

    SharedPtr<CountingObserver> observer = create_shared<CountingObserver>();
    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(create_unique<DummyGreedyStrategy>());
    strategies.push_back(create_unique<DummyGreedyStrategy>());

    SimulationLayer& layer = app.push_layer<SimulationLayer>();
    StartSimulationEvent start(create_unique<DummyGame>(10), std::move(strategies), /*match_count=*/1, observer, /*benchmark=*/false);
    app.post_event(start);

    app.run();

    CHECK(observer->turns > 1);
    CHECK(observer->finished_frames == 1);
    CHECK(layer.result().matches == 1);
}

TEST_CASE("A lingering SimulationLayer holds a finished match until a restart is requested")
{
    Application app({ 0, nullptr });

    SharedPtr<RestartingObserver> observer = create_shared<RestartingObserver>();
    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(create_unique<DummyGreedyStrategy>());
    strategies.push_back(create_unique<DummyGreedyStrategy>());

    SimulationLayer& layer = app.push_layer<SimulationLayer>();
    StartSimulationEvent start(create_unique<DummyGame>(10), std::move(strategies), /*match_count=*/1, observer, /*benchmark=*/false, /*linger=*/true);
    app.post_event(start);

    app.run();

    CHECK(observer->finished_frames == 3);
    CHECK(layer.result().matches == 1);
}
