#pragma once

#include "Oryx/Assets/AssetId.h"

namespace oryx
{

template<typename T>
struct AssetHandle
{
    AssetId id;
    uint32_t generation = 0;
};

template<typename T>
[[nodiscard]] constexpr bool operator==(const AssetHandle<T>& a, const AssetHandle<T>& b)
{
    return a.id == b.id && a.generation == b.generation;
}

template<typename T>
[[nodiscard]] constexpr bool operator!=(const AssetHandle<T>& a, const AssetHandle<T>& b)
{
    return !(a == b);
}

template<typename T>
[[nodiscard]] constexpr bool is_null(const AssetHandle<T>& handle)
{
    return is_null(handle.id);
}

} // namespace oryx
