#pragma once

#include "Oryx/Game/ActionId.h"

#include <vector>

namespace oryx
{

class IActionFeatures
{
public:
    virtual ~IActionFeatures() = default;

    virtual std::vector<int32_t> decode(ActionId action) const = 0;
};

} // namespace oryx
