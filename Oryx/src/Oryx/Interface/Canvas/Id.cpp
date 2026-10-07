#include "oxpch.h"
#include "Oryx/Interface/Canvas/Id.h"

namespace oryx
{

namespace
{

constexpr uint64_t k_fnv_offset = 14695981039346656037ull;
constexpr uint64_t k_fnv_prime = 1099511628211ull;

uint64_t mix(uint64_t hash, uint8_t byte)
{
    return (hash ^ byte) * k_fnv_prime;
}

Id finish(uint64_t hash)
{
    return { hash == 0 ? 1 : hash };
}

} // namespace

Id make_id(std::string_view label, Id parent)
{
    uint64_t hash = k_fnv_offset;
    for (uint32_t shift = 0; shift < 64; shift += 8)
    {
        hash = mix(hash, static_cast<uint8_t>(parent.value >> shift));
    }
    for (char character : label)
    {
        hash = mix(hash, static_cast<uint8_t>(character));
    }
    return finish(hash);
}

Id make_index_id(uint64_t index, Id parent)
{
    uint64_t hash = k_fnv_offset ^ 0x9E3779B97F4A7C15ull;
    for (uint32_t shift = 0; shift < 64; shift += 8)
    {
        hash = mix(hash, static_cast<uint8_t>(parent.value >> shift));
    }
    for (uint32_t shift = 0; shift < 64; shift += 8)
    {
        hash = mix(hash, static_cast<uint8_t>(index >> shift));
    }
    return finish(hash);
}

} // namespace oryx
