#include "oxpch.h"
#include "Oryx/Simulation/SeedSequence.h"

namespace oryx
{

namespace
{

uint64_t splitmix64(uint64_t value)
{
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31);
}

} // namespace

uint64_t hash_string(std::string_view text)
{
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (char character : text)
    {
        hash ^= static_cast<uint8_t>(character);
        hash *= 0x100000001B3ULL;
    }
    return hash;
}

uint64_t derive_seed(uint64_t master, std::string_view key, SeedRole role, uint32_t seat, uint32_t repeat)
{
    uint64_t seed = splitmix64(master ^ hash_string(key));
    seed = splitmix64(seed + static_cast<uint64_t>(role));
    seed = splitmix64(seed + seat);
    return splitmix64(seed + repeat);
}

} // namespace oryx
