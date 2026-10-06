#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

struct AssetId
{
    uint32_t value = 0;
};

[[nodiscard]] constexpr bool operator==(AssetId a, AssetId b) { return a.value == b.value; }
[[nodiscard]] constexpr bool operator!=(AssetId a, AssetId b) { return a.value != b.value; }
[[nodiscard]] constexpr bool is_null(AssetId id) { return id.value == 0; }

} // namespace oryx
