#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

struct RHICapabilities
{
    std::string name;
    uint32_t max_texture_size = 0;
    uint32_t frames_in_flight = 1;
};

} // namespace oryx
