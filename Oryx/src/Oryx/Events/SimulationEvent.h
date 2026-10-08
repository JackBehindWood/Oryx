#pragma once

#include "Oryx/Benchmark/MemoryBenchmarkRunner.h"
#include "Oryx/Events/Event.h"
#include "Oryx/Simulation/BatchRunner.h"
#include "Oryx/Simulation/SimulationLayer.h"

namespace oryx
{

class StartSimulationEvent : public Event
{
public:
    StartSimulationEvent(UniquePtr<IGame> game,
                          SmallVector<UniquePtr<IStrategy>, 2> strategies,
                          int32_t match_count,
                          SharedPtr<ITurnObserver> observer,
                          bool benchmark,
                          bool linger = false,
                          IDecisionObserver* decision_observer = nullptr)
        : m_game(std::move(game))
        , m_strategies(std::move(strategies))
        , m_match_count(match_count)
        , m_observer(std::move(observer))
        , m_benchmark(benchmark)
        , m_linger(linger)
        , m_decision_observer(decision_observer)
    {
    }

    UniquePtr<IGame> take_game() { return std::move(m_game); }
    SmallVector<UniquePtr<IStrategy>, 2> take_strategies() { return std::move(m_strategies); }
    [[nodiscard]] int32_t match_count() const { return m_match_count; }
    SharedPtr<ITurnObserver> take_observer() { return std::move(m_observer); }
    [[nodiscard]] bool benchmark() const { return m_benchmark; }
    [[nodiscard]] bool linger() const { return m_linger; }
    // Non-owning; must outlive the simulation, as Match::set_observer requires.
    [[nodiscard]] IDecisionObserver* decision_observer() const { return m_decision_observer; }

    OX_EVENT_CLASS_TYPE(StartSimulation)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    UniquePtr<IGame> m_game;
    SmallVector<UniquePtr<IStrategy>, 2> m_strategies;
    int32_t m_match_count;
    SharedPtr<ITurnObserver> m_observer;
    bool m_benchmark;
    bool m_linger;
    IDecisionObserver* m_decision_observer;
};

// Asks the board layer for a new match; an empty name keeps the current game or opponent. Applied at the layer's next update.
class StartMatchEvent : public Event
{
public:
    StartMatchEvent(std::string game, std::string opponent)
        : m_game(std::move(game))
        , m_opponent(std::move(opponent))
    {
    }

    [[nodiscard]] const std::string& game() const { return m_game; }
    [[nodiscard]] const std::string& opponent() const { return m_opponent; }

    OX_EVENT_CLASS_TYPE(StartMatch)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    std::string m_game;
    std::string m_opponent;
};

// Starts the next match of a lingering simulation once the finished one has been shown.
class RestartSimulationEvent : public Event
{
public:
    RestartSimulationEvent() = default;
    // `seat_order[seat]` is the index, among the strategies the simulation started with, of the one that plays `seat` in the next match.
    explicit RestartSimulationEvent(SmallVector<uint32_t, 2> seat_order)
        : m_seat_order(std::move(seat_order))
    {
    }

    [[nodiscard]] const SmallVector<uint32_t, 2>& seat_order() const { return m_seat_order; }

    OX_EVENT_CLASS_TYPE(RestartSimulation)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    SmallVector<uint32_t, 2> m_seat_order;
};

class SimulationCompleteEvent : public Event
{
public:
    SimulationCompleteEvent(MemoryBenchmarkRunner::MemoryResults results, bool benchmark)
        : m_results(std::move(results))
        , m_benchmark(benchmark)
    {
    }

    [[nodiscard]] const BatchResult& result() const { return m_results.outcome; }
    [[nodiscard]] const MemoryBenchmarkRunner::MemoryResults& results() const { return m_results; }
    [[nodiscard]] bool benchmark() const { return m_benchmark; }

    OX_EVENT_CLASS_TYPE(SimulationComplete)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    MemoryBenchmarkRunner::MemoryResults m_results;
    bool m_benchmark;
};

} // namespace oryx
