#pragma once

#include "Oryx/Core/Timer.h"
#include "Oryx/Core/Base.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Memory/MemoryStats.h"
#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Simulation/ITurnObserver.h"
#include "Oryx/Simulation/BatchRunner.h"
#include "Oryx/Simulation/Match.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class StartSimulationEvent;
class RestartSimulationEvent;

class SimulationLayer : public Layer
{
public:
    explicit SimulationLayer(bool benchmark = false);

    void event(Event& event) override;
    void update(double delta_time) override;

    [[nodiscard]] const BatchResult& result() const { return m_result; }

private:
    bool on_start_simulation(StartSimulationEvent& event);
    bool on_restart_simulation(RestartSimulationEvent& event);
    void apply_seat_order();
    void reset_run();

    UniquePtr<IGame> m_game;
    SmallVector<UniquePtr<IStrategy>, 2> m_strategy_storage;
    SmallVector<IStrategy*, 2> m_strategies;
    // Applied when the next match is created, so a finished match keeps the strategies it was played with.
    SmallVector<uint32_t, 2> m_next_seat_order;
    int32_t m_match_count = 0;
    SharedPtr<ITurnObserver> m_observer;
    IDecisionObserver* m_decision_observer = nullptr;
    // A lingering simulation keeps a finished match until a restart is requested and runs until the application closes.
    bool m_linger = false;
    bool m_restart_requested = false;
    bool m_benchmark;
    Timer m_timer;
    MemoryStats m_memory_before;

    UniquePtr<Match> m_match;
    int32_t m_completed = 0;
    BatchResult m_result;
};

} // namespace oryx
