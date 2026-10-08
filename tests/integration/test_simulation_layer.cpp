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

namespace
{

class SeatSwappingObserver : public ITurnObserver
{
public:
    void on_turn(IState& state) override
    {
        if (!state.is_terminal())
        {
            return;
        }
        ++finished_frames;
        if (finished_frames == 1)
        {
            SmallVector<uint32_t, 2> order;
            order.push_back(1);
            order.push_back(0);
            RestartSimulationEvent restart(std::move(order));
            Application::Get().post_event(restart);
        }
        else if (finished_frames >= 2)
        {
            Application::Get().close();
        }
    }

    void on_match_start() override { ++matches_started; }

    int32_t finished_frames = 0;
    int32_t matches_started = 0;
};

} // namespace

TEST_CASE("A restart can seat the strategies in a new order and tells the observer each match has started")
{
    Application app({ 0, nullptr });

    SharedPtr<SeatSwappingObserver> observer = create_shared<SeatSwappingObserver>();
    std::vector<PlayerId> first_strategy_seats;
    std::vector<PlayerId> second_strategy_seats;
    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(make_external([&](const Context& context) -> ActionId
    {
        first_strategy_seats.push_back(context.state().current_player());
        return context.state().legal_actions()[0];
    }));
    strategies.push_back(make_external([&](const Context& context) -> ActionId
    {
        second_strategy_seats.push_back(context.state().current_player());
        return context.state().legal_actions()[0];
    }));

    app.push_layer<SimulationLayer>();
    StartSimulationEvent start(create_unique<DummyGame>(10), std::move(strategies), /*match_count=*/1, observer, /*benchmark=*/false, /*linger=*/true);
    app.post_event(start);
    app.run();

    REQUIRE_FALSE(first_strategy_seats.empty());
    REQUIRE_FALSE(second_strategy_seats.empty());
    CHECK(first_strategy_seats.front() == 0);
    CHECK(second_strategy_seats.front() == 1);
    CHECK(first_strategy_seats.back() == 1);
    CHECK(second_strategy_seats.back() == 0);
    CHECK(observer->matches_started == 2);
}

namespace
{

class MatchCountingObserver : public IDecisionObserver
{
public:
    void on_decision(const IState&, const Decision&) override { ++decisions; }
    void on_match_start() override { ++matches_started; }

    int32_t decisions = 0;
    int32_t matches_started = 0;
};

SmallVector<UniquePtr<IStrategy>, 2> counting_strategies(int32_t& first_calls, int32_t& second_calls)
{
    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(make_external([&first_calls](const Context& context) -> ActionId { ++first_calls; return context.state().legal_actions()[0]; }));
    strategies.push_back(make_external([&second_calls](const Context& context) -> ActionId { ++second_calls; return context.state().legal_actions()[0]; }));
    return strategies;
}

} // namespace

TEST_CASE("A second StartSimulationEvent replaces the first run instead of appending to it")
{
    Application app({ 0, nullptr });
    SimulationLayer& layer = app.push_layer<SimulationLayer>();

    int32_t old_first = 0;
    int32_t old_second = 0;
    StartSimulationEvent first(create_unique<DummyGame>(10), counting_strategies(old_first, old_second), /*match_count=*/5, nullptr, /*benchmark=*/false);
    app.post_event(first);
    for (int32_t i = 0; i < 3; ++i)
    {
        layer.update(0.0);
    }
    int32_t old_first_before = old_first;
    int32_t old_second_before = old_second;
    CHECK(old_first_before + old_second_before > 0);

    int32_t new_first = 0;
    int32_t new_second = 0;
    StartSimulationEvent second(create_unique<DummyGame>(10), counting_strategies(new_first, new_second), /*match_count=*/1, nullptr, /*benchmark=*/false);
    app.post_event(second);
    CHECK(layer.result().matches == 0);

    for (int32_t i = 0; i < 100 && !app.closing(); ++i)
    {
        layer.update(0.0);
    }

    CHECK(layer.result().matches == 1);
    CHECK(layer.result().wins.size() == 2);
    CHECK(new_first > 0);
    CHECK(new_second > 0);
    CHECK(old_first == old_first_before);
    CHECK(old_second == old_second_before);
}

TEST_CASE("The decision observer of a start event is told about every match it will see")
{
    Application app({ 0, nullptr });
    SimulationLayer& layer = app.push_layer<SimulationLayer>();

    MatchCountingObserver observer;
    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(create_unique<DummyGreedyStrategy>());
    strategies.push_back(create_unique<DummyGreedyStrategy>());
    StartSimulationEvent start(create_unique<DummyGame>(10), std::move(strategies), /*match_count=*/3, nullptr, /*benchmark=*/false, /*linger=*/false, &observer);
    app.post_event(start);
    app.run();

    CHECK(layer.result().matches == 3);
    CHECK(observer.matches_started == 3);
}

TEST_CASE("A dashboard feed receives the labelled decisions of Tic-Tac-Toe against Minimax, one match at a time")
{
    Application app({ 0, nullptr });
    SimulationLayer& layer = app.push_layer<SimulationLayer>();

    DashboardFeed feed;
    SmallVector<UniquePtr<IStrategy>, 2> strategies;
    strategies.push_back(StrategyRegistry::create("minimax"));
    strategies.push_back(StrategyRegistry::create("minimax"));
    StartSimulationEvent start(GameRegistry::create("tictactoe"), std::move(strategies), /*match_count=*/2, nullptr, /*benchmark=*/false, /*linger=*/false, &feed);
    app.post_event(start);
    app.run();

    REQUIRE(layer.result().matches == 2);
    REQUIRE(feed.size() > 0);
    CHECK(feed.match_index() == 1);
    CHECK(feed.view(0).record->match_index == 0);
    RecordView last = feed.view(feed.size() - 1);
    CHECK(last.record->match_index == 1);
    CHECK(last.record->score_count > 0);
    CHECK(last.record->chosen_label.length > 0);
}
