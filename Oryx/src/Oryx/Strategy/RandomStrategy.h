#pragma once

#include "Oryx/Core/Random.h"
#include "Oryx/Strategy/IStrategy.h"

namespace oryx
{

class RandomStrategy : public IStrategy
{
public:
    RandomStrategy() = default;
    explicit RandomStrategy(uint64_t seed) : m_random(seed) {}
    explicit RandomStrategy(const Params& params);

    ActionId decide(const Context& context) override;

private:
    Random m_random;
};

} // namespace oryx
