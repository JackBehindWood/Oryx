#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

// Uniform random number generator
class Random
{
public:
    Random();
    explicit Random(uint64_t seed);
    ~Random();

    void seed(uint64_t seed);

    int64_t get_int(int64_t min=0, int64_t max=std::numeric_limits<int64_t>::max());

    double get_double(double min = 0.0, double max = 1.0);

    bool get_bool(double p = 0.5);

private:
    struct Impl;
    UniquePtr<Impl> m_impl;
};

} // namespace oryx
