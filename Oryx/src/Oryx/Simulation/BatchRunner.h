#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/IGame.h"
#include "Oryx/Game/Outcome.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

struct BatchResult
{
    int32_t matches = 0;
    SmallVector<int32_t, 2> wins;
    int32_t draws = 0;
    Rewards<double> rewards{ 0 };
};

void accumulate(BatchResult& result, const Outcome& outcome);

class BatchRunner
{
public:
    BatchRunner(const IGame& game, SmallVector<IStrategy*, 2> strategies);

    BatchResult run(int32_t match_count);

private:
    const IGame& m_game;
    SmallVector<IStrategy*, 2> m_strategies;
};

} // namespace oryx
