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
                          SimulationLayer::TurnObserver on_turn,
                          bool benchmark)
        : m_game(std::move(game))
        , m_strategies(std::move(strategies))
        , m_match_count(match_count)
        , m_on_turn(std::move(on_turn))
        , m_benchmark(benchmark)
    {
    }

    UniquePtr<IGame> take_game() { return std::move(m_game); }
    SmallVector<UniquePtr<IStrategy>, 2> take_strategies() { return std::move(m_strategies); }
    [[nodiscard]] int32_t match_count() const { return m_match_count; }
    SimulationLayer::TurnObserver take_on_turn() { return std::move(m_on_turn); }
    [[nodiscard]] bool benchmark() const { return m_benchmark; }

    OX_EVENT_CLASS_TYPE(StartSimulation)
    OX_EVENT_CLASS_CATEGORY(EventCategoryApplication)

private:
    UniquePtr<IGame> m_game;
    SmallVector<UniquePtr<IStrategy>, 2> m_strategies;
    int32_t m_match_count;
    SimulationLayer::TurnObserver m_on_turn;
    bool m_benchmark;
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
