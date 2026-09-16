#include "oxpch.h"
#include "Oryx/Core/Random.h"

#include <random>

namespace oryx
{

struct Random::Impl
{
    std::mt19937_64 engine;
};

Random::Random() : m_impl(create_unique<Impl>())
{
    m_impl->engine.seed(std::random_device{}());
}

Random::Random(uint64_t seed) : m_impl(create_unique<Impl>())
{
    m_impl->engine.seed(seed);
}

Random::~Random() = default;

void Random::seed(uint64_t seed)
{
    m_impl->engine.seed(seed);
}

int64_t Random::get_int(int64_t min, int64_t max)
{
    std::uniform_int_distribution<int64_t> dist(min, max);
    return dist(m_impl->engine);
}

double Random::get_double(double min, double max)
{
    std::uniform_real_distribution<double> dist(min, max);
    return dist(m_impl->engine);
}

bool Random::get_bool(double p)
{
    std::bernoulli_distribution dist(p);
    return dist(m_impl->engine);
}

} // namespace oryx
