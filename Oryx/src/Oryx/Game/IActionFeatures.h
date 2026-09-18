#pragma once

#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/ActionId.h"

namespace oryx
{

class IActionFeatures
{
public:
    virtual ~IActionFeatures() = default;

    // Always a fixed-shape coordinate/feature tuple (e.g. row/col).
    virtual SmallVector<int32_t, 2> decode(ActionId action) const = 0;
};

} // namespace oryx
