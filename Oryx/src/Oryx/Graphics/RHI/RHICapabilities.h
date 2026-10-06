#pragma once

#include "Oryx/Core/Base.h"

namespace oryx
{

struct RHICapabilities
{
    std::string name;
    uint32_t max_texture_size = 0;
    uint32_t frames_in_flight = 1;
    uint32_t max_texture_bindings = 0;
    // True when pipeline creation compares the layout and vertex input with the backend's own shader reflection (not in Dist builds).
    bool validates_shader_interface = false;
};

} // namespace oryx
