#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Shaders/Compiler/ShaderCompiler.h"

namespace oryx
{

// The default domain: one permutation, no defines.
struct SinglePermutation
{
    static std::vector<ShaderDefine> defines_for(uint32_t) { return {}; }
    static bool should_compile(uint32_t permutation) { return permutation == 0; }
};

// One define with an enumerated value per permutation id; `Dimension` supplies `NAME` and `VALUES[]`.
template<typename Dimension>
struct ShaderPermutationDomain
{
    static constexpr uint32_t COUNT = sizeof(Dimension::VALUES) / sizeof(Dimension::VALUES[0]);

    [[nodiscard]] static uint32_t value(uint32_t permutation)
    {
        if (permutation >= COUNT)
        {
            throw Error("Shader permutation is out of range", std::to_string(permutation));
        }
        return Dimension::VALUES[permutation];
    }

    // The permutation of the largest value not above `limit` (a device capability); Unreal's pass code sets its permutation vector from caps the same way.
    [[nodiscard]] static uint32_t fitting(uint32_t limit)
    {
        uint32_t best = COUNT;
        for (uint32_t permutation = 0; permutation < COUNT; ++permutation)
        {
            if (Dimension::VALUES[permutation] <= limit && (best == COUNT || Dimension::VALUES[permutation] > Dimension::VALUES[best]))
            {
                best = permutation;
            }
        }
        if (best == COUNT)
        {
            throw Error("No shader permutation fits the limit", std::to_string(limit));
        }
        return best;
    }

    static std::vector<ShaderDefine> defines_for(uint32_t permutation) { return { { Dimension::NAME, std::to_string(value(permutation)) } }; }
    static bool should_compile(uint32_t permutation) { return permutation < COUNT; }
};

} // namespace oryx
