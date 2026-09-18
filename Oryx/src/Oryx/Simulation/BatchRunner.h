#pragma once

#include "Oryx/Game/IGame.h"
#include "Oryx/Game/Outcome.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

struct BatchResult
{
    int32_t matches = 0;
    std::vector<int32_t> wins;
    int32_t draws = 0;
    Rewards<double> rewards{ 0 };
};

void accumulate(BatchResult& result, const Outcome& outcome);

class BatchRunner
{
public:
    BatchRunner(const IGame& game, std::vector<IStrategy*> strategies);

    BatchResult run(int32_t match_count);

private:
    const IGame& m_game;
    std::vector<IStrategy*> m_strategies;
};

} // namespace oryx
