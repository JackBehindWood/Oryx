#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Layer.h"
#include "Oryx/Containers/SmallVector.h"
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
                     SmallVector<UniquePtr<IStrategy>, 2> strategies,
                     int32_t match_count = 1,
                     TurnObserver on_turn = nullptr);

    void attach() override;
    void update() override;

    [[nodiscard]] const BatchResult& result() const { return m_result; }

private:
    UniquePtr<IGame> m_game;
    SmallVector<UniquePtr<IStrategy>, 2> m_strategy_storage;
    SmallVector<IStrategy*, 2> m_strategies;
    int32_t m_match_count;
    TurnObserver m_on_turn;

    UniquePtr<Match> m_match;
    int32_t m_completed = 0;
    BatchResult m_result;
};

} // namespace oryx
