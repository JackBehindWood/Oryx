#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Simulation/BatchRunner.h"
#include "Oryx/Simulation/Match.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class SimulationLayer : public Layer
{
public:
    using TurnObserver = std::function<void(IState& state)>;

    SimulationLayer(UniquePtr<IGame> game,
                     std::vector<UniquePtr<IStrategy>> strategies,
                     int32_t match_count = 1,
                     TurnObserver on_turn = nullptr);

    void attach() override;
    void update() override;

    [[nodiscard]] const BatchResult& result() const { return m_result; }

private:
    UniquePtr<IGame> m_game;
    std::vector<UniquePtr<IStrategy>> m_strategy_storage;
    std::vector<IStrategy*> m_strategies;
    int32_t m_match_count;
    TurnObserver m_on_turn;

    UniquePtr<Match> m_match;
    int32_t m_completed = 0;
    BatchResult m_result;
};

} // namespace oryx
