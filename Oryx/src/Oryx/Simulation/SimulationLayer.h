#pragma once

#include "Oryx/Benchmark/Timer.h"
#include "Oryx/Core/Base.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Debug/MemoryTracker.h"
#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Simulation/BatchRunner.h"
#include "Oryx/Simulation/Match.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class StartSimulationEvent;

class SimulationLayer : public Layer
{
public:
    using TurnObserver = std::function<void(IState& state)>;

    explicit SimulationLayer(bool benchmark = false);

    void event(Event& event) override;
    void update() override;

    [[nodiscard]] const BatchResult& result() const { return m_result; }

private:
    bool on_start_simulation(StartSimulationEvent& event);

    UniquePtr<IGame> m_game;
    SmallVector<UniquePtr<IStrategy>, 2> m_strategy_storage;
    SmallVector<IStrategy*, 2> m_strategies;
    int32_t m_match_count = 0;
    TurnObserver m_on_turn;
    bool m_benchmark;
    Timer m_timer;
    MemoryStats m_memory_before;

    UniquePtr<Match> m_match;
    int32_t m_completed = 0;
    BatchResult m_result;
};

} // namespace oryx
