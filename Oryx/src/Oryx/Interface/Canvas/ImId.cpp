#include "oxpch.h"
#include "Oryx/Interface/Canvas/ImId.h"

namespace oryx
{

namespace
{

uint64_t mix(uint64_t hash, uint8_t byte)
{
    return (hash ^ byte) * k_im_fnv_prime;
}

ImId finish(uint64_t hash)
{
    return ImId{ hash == ImId::none ? 1 : hash };
}

} // namespace

ImId make_im_id(std::string_view label, ImId parent)
{
    uint64_t hash = k_im_fnv_offset;
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

ImId make_im_index_id(uint64_t index, ImId parent)
{
    uint64_t hash = k_im_fnv_offset ^ 0x9E3779B97F4A7C15ull;
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
