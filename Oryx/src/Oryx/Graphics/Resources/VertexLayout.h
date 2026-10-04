#pragma once

#include "Oryx/Graphics/RHI/RHIRenderState.h"

namespace oryx
{

// One interleaved vertex stream: the attributes it carries and the stride between vertices.
struct VertexLayout
{
    std::vector<RHIVertexAttribute> attributes;
    uint32_t stride = 0;
};

} // namespace oryx
