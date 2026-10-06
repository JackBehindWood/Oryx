#pragma once

#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/RHI/RHIFlags.h"
#include "Oryx/Graphics/RHI/RHIFormat.h"

namespace oryx
{

inline constexpr uint32_t RHI_MAX_VERTEX_SLOTS = 4;
// Compile-time ceiling for texture-array sizes; the device's own limit is RHICapabilities::max_texture_bindings.
inline constexpr uint32_t RHI_MAX_TEXTURE_BINDINGS = 32;
inline constexpr uint32_t RHI_MAX_COLOUR_TARGETS = 8;

[[nodiscard]] constexpr uint32_t rhi_vertex_format_bytes(RHIVertexFormat format)
{
    switch (format)
    {
    case RHIVertexFormat::Float: return 4;
    case RHIVertexFormat::Float2: return 8;
    case RHIVertexFormat::Float3: return 12;
    case RHIVertexFormat::Float4: return 16;
    case RHIVertexFormat::Half2: return 4;
    case RHIVertexFormat::Half4: return 8;
    case RHIVertexFormat::UByte4Norm: return 4;
    case RHIVertexFormat::UInt:
    case RHIVertexFormat::Int: return 4;
    case RHIVertexFormat::UInt2:
    case RHIVertexFormat::Int2: return 8;
    case RHIVertexFormat::UInt3:
    case RHIVertexFormat::Int3: return 12;
    case RHIVertexFormat::UInt4:
    case RHIVertexFormat::Int4: return 16;
    }
    return 0;
}

[[nodiscard]] constexpr uint32_t rhi_vertex_format_components(RHIVertexFormat format)
{
    switch (format)
    {
    case RHIVertexFormat::Float:
    case RHIVertexFormat::UInt:
    case RHIVertexFormat::Int: return 1;
    case RHIVertexFormat::Float2:
    case RHIVertexFormat::Half2:
    case RHIVertexFormat::UInt2:
    case RHIVertexFormat::Int2: return 2;
    case RHIVertexFormat::Float3:
    case RHIVertexFormat::UInt3:
    case RHIVertexFormat::Int3: return 3;
    case RHIVertexFormat::Float4:
    case RHIVertexFormat::Half4:
    case RHIVertexFormat::UByte4Norm:
    case RHIVertexFormat::UInt4:
    case RHIVertexFormat::Int4: return 4;
    }
    return 0;
}

// True when a shader reads the format as floats (float formats, halves and normalised bytes), false for the integer formats.
[[nodiscard]] constexpr bool rhi_vertex_format_is_float(RHIVertexFormat format)
{
    switch (format)
    {
    case RHIVertexFormat::UInt:
    case RHIVertexFormat::UInt2:
    case RHIVertexFormat::UInt3:
    case RHIVertexFormat::UInt4:
    case RHIVertexFormat::Int:
    case RHIVertexFormat::Int2:
    case RHIVertexFormat::Int3:
    case RHIVertexFormat::Int4: return false;
    default: return true;
    }
}

struct RHIVertexAttribute
{
    uint32_t location = 0;
    RHIVertexFormat format = RHIVertexFormat::Float3;
    uint32_t offset = 0;
    uint32_t slot = 0;
};

struct RHIVertexStream
{
    uint32_t stride = 0;
    RHIVertexStep step_function = RHIVertexStep::PerVertex;
    uint32_t step_rate = 1;
};

// attributes is only read during creation.
struct RHIVertexInput
{
    const RHIVertexAttribute* attributes = nullptr;
    uint32_t attribute_count = 0;
    RHIVertexStream streams[RHI_MAX_VERTEX_SLOTS];
};

struct RHIRasterizerState
{
    RHICullMode cull = RHICullMode::None;
    RHIFrontFace front_face = RHIFrontFace::CounterClockwise;
    RHIFillMode fill = RHIFillMode::Solid;
};

struct RHIBlendState
{
    bool enabled = false;
    RHIBlendFactor src_colour = RHIBlendFactor::One;
    RHIBlendFactor dst_colour = RHIBlendFactor::Zero;
    RHIBlendOp colour_op = RHIBlendOp::Add;
    RHIBlendFactor src_alpha = RHIBlendFactor::One;
    RHIBlendFactor dst_alpha = RHIBlendFactor::Zero;
    RHIBlendOp alpha_op = RHIBlendOp::Add;
    RHIColourWriteMask write_mask = RHIColourWriteMask::All;
};

[[nodiscard]] constexpr RHIBlendState rhi_blend_opaque()
{
    return {};
}

[[nodiscard]] constexpr RHIBlendState rhi_blend_alpha()
{
    return { true, RHIBlendFactor::SrcAlpha, RHIBlendFactor::OneMinusSrcAlpha, RHIBlendOp::Add, RHIBlendFactor::One, RHIBlendFactor::OneMinusSrcAlpha, RHIBlendOp::Add, RHIColourWriteMask::All };
}

[[nodiscard]] constexpr RHIBlendState rhi_blend_additive()
{
    return { true, RHIBlendFactor::SrcAlpha, RHIBlendFactor::One, RHIBlendOp::Add, RHIBlendFactor::One, RHIBlendFactor::One, RHIBlendOp::Add, RHIColourWriteMask::All };
}

struct RHIStencilFace
{
    RHIStencilOp fail = RHIStencilOp::Keep;
    RHIStencilOp depth_fail = RHIStencilOp::Keep;
    RHIStencilOp pass = RHIStencilOp::Keep;
    RHICompare compare = RHICompare::Always;
};

struct RHIDepthStencilState
{
    bool depth_test = false;
    bool depth_write = false;
    RHICompare depth_compare = RHICompare::Less;
    bool stencil_test = false;
    RHIStencilFace front;
    RHIStencilFace back;
    uint8_t stencil_read_mask = 0xFF;
    uint8_t stencil_write_mask = 0xFF;
};

struct RHIViewportState
{
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float min_depth = 0.0f;
    float max_depth = 1.0f;
};

struct RHIScissorRect
{
    int32_t x = 0;
    int32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

} // namespace oryx
