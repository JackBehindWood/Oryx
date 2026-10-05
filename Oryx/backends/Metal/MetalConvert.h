#pragma once

#include "MetalApi.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIRenderPass.h"
#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx::metal
{

inline constexpr uint32_t METAL_VERTEX_STREAM_BASE = 16;

[[nodiscard]] inline MTL::PixelFormat to_mtl(RHIFormat format)
{
    switch (format)
    {
    case RHIFormat::R8Unorm: return MTL::PixelFormatR8Unorm;
    case RHIFormat::RGBA8Unorm: return MTL::PixelFormatRGBA8Unorm;
    case RHIFormat::BGRA8Unorm: return MTL::PixelFormatBGRA8Unorm;
    case RHIFormat::RGBA16Float: return MTL::PixelFormatRGBA16Float;
    case RHIFormat::Depth32Float: return MTL::PixelFormatDepth32Float;
    case RHIFormat::Undefined: break;
    }
    throw Error("RHI format is undefined");
}

[[nodiscard]] inline MTL::SamplerMinMagFilter to_mtl(RHIFilter filter)
{
    return filter == RHIFilter::Nearest ? MTL::SamplerMinMagFilterNearest : MTL::SamplerMinMagFilterLinear;
}

[[nodiscard]] inline MTL::SamplerAddressMode to_mtl(RHIAddressMode mode)
{
    switch (mode)
    {
    case RHIAddressMode::Clamp: return MTL::SamplerAddressModeClampToEdge;
    case RHIAddressMode::Repeat: return MTL::SamplerAddressModeRepeat;
    case RHIAddressMode::Mirror: return MTL::SamplerAddressModeMirrorRepeat;
    }
    throw Error("RHI address mode is invalid");
}

[[nodiscard]] inline MTL::TextureUsage to_mtl(RHITextureUsage usage)
{
    MTL::TextureUsage result = MTL::TextureUsageUnknown;
    if (has_flag(usage, RHITextureUsage::Sampled))
    {
        result |= MTL::TextureUsageShaderRead;
    }
    if (has_flag(usage, RHITextureUsage::RenderTarget) || has_flag(usage, RHITextureUsage::DepthStencil))
    {
        result |= MTL::TextureUsageRenderTarget;
    }
    return result;
}

[[nodiscard]] inline MTL::VertexFormat to_mtl(RHIVertexFormat format)
{
    switch (format)
    {
    case RHIVertexFormat::Float: return MTL::VertexFormatFloat;
    case RHIVertexFormat::Float2: return MTL::VertexFormatFloat2;
    case RHIVertexFormat::Float3: return MTL::VertexFormatFloat3;
    case RHIVertexFormat::Float4: return MTL::VertexFormatFloat4;
    case RHIVertexFormat::Half2: return MTL::VertexFormatHalf2;
    case RHIVertexFormat::Half4: return MTL::VertexFormatHalf4;
    case RHIVertexFormat::UByte4Norm: return MTL::VertexFormatUChar4Normalized;
    case RHIVertexFormat::UInt: return MTL::VertexFormatUInt;
    case RHIVertexFormat::UInt2: return MTL::VertexFormatUInt2;
    case RHIVertexFormat::UInt3: return MTL::VertexFormatUInt3;
    case RHIVertexFormat::UInt4: return MTL::VertexFormatUInt4;
    case RHIVertexFormat::Int: return MTL::VertexFormatInt;
    case RHIVertexFormat::Int2: return MTL::VertexFormatInt2;
    case RHIVertexFormat::Int3: return MTL::VertexFormatInt3;
    case RHIVertexFormat::Int4: return MTL::VertexFormatInt4;
    }
    throw Error("RHI vertex format is invalid");
}

[[nodiscard]] inline MTL::PrimitiveType to_mtl(RHITopology topology)
{
    switch (topology)
    {
    case RHITopology::Points: return MTL::PrimitiveTypePoint;
    case RHITopology::Lines: return MTL::PrimitiveTypeLine;
    case RHITopology::LineStrip: return MTL::PrimitiveTypeLineStrip;
    case RHITopology::Triangles: return MTL::PrimitiveTypeTriangle;
    case RHITopology::TriangleStrip: return MTL::PrimitiveTypeTriangleStrip;
    }
    throw Error("RHI topology is invalid");
}

[[nodiscard]] inline MTL::PrimitiveTopologyClass to_mtl_class(RHITopology topology)
{
    switch (topology)
    {
    case RHITopology::Points: return MTL::PrimitiveTopologyClassPoint;
    case RHITopology::Lines:
    case RHITopology::LineStrip: return MTL::PrimitiveTopologyClassLine;
    case RHITopology::Triangles:
    case RHITopology::TriangleStrip: return MTL::PrimitiveTopologyClassTriangle;
    }
    throw Error("RHI topology is invalid");
}

[[nodiscard]] inline MTL::CullMode to_mtl(RHICullMode mode)
{
    switch (mode)
    {
    case RHICullMode::None: return MTL::CullModeNone;
    case RHICullMode::Front: return MTL::CullModeFront;
    case RHICullMode::Back: return MTL::CullModeBack;
    }
    throw Error("RHI cull mode is invalid");
}

[[nodiscard]] inline MTL::Winding to_mtl(RHIFrontFace face)
{
    return face == RHIFrontFace::Clockwise ? MTL::WindingClockwise : MTL::WindingCounterClockwise;
}

[[nodiscard]] inline MTL::TriangleFillMode to_mtl(RHIFillMode mode)
{
    return mode == RHIFillMode::Wireframe ? MTL::TriangleFillModeLines : MTL::TriangleFillModeFill;
}

[[nodiscard]] inline MTL::BlendFactor to_mtl(RHIBlendFactor factor)
{
    switch (factor)
    {
    case RHIBlendFactor::Zero: return MTL::BlendFactorZero;
    case RHIBlendFactor::One: return MTL::BlendFactorOne;
    case RHIBlendFactor::SrcColour: return MTL::BlendFactorSourceColor;
    case RHIBlendFactor::OneMinusSrcColour: return MTL::BlendFactorOneMinusSourceColor;
    case RHIBlendFactor::DstColour: return MTL::BlendFactorDestinationColor;
    case RHIBlendFactor::OneMinusDstColour: return MTL::BlendFactorOneMinusDestinationColor;
    case RHIBlendFactor::SrcAlpha: return MTL::BlendFactorSourceAlpha;
    case RHIBlendFactor::OneMinusSrcAlpha: return MTL::BlendFactorOneMinusSourceAlpha;
    case RHIBlendFactor::DstAlpha: return MTL::BlendFactorDestinationAlpha;
    case RHIBlendFactor::OneMinusDstAlpha: return MTL::BlendFactorOneMinusDestinationAlpha;
    }
    throw Error("RHI blend factor is invalid");
}

[[nodiscard]] inline MTL::BlendOperation to_mtl(RHIBlendOp op)
{
    switch (op)
    {
    case RHIBlendOp::Add: return MTL::BlendOperationAdd;
    case RHIBlendOp::Subtract: return MTL::BlendOperationSubtract;
    case RHIBlendOp::ReverseSubtract: return MTL::BlendOperationReverseSubtract;
    case RHIBlendOp::Min: return MTL::BlendOperationMin;
    case RHIBlendOp::Max: return MTL::BlendOperationMax;
    }
    throw Error("RHI blend op is invalid");
}

[[nodiscard]] inline MTL::ColorWriteMask to_mtl(RHIColourWriteMask mask)
{
    MTL::ColorWriteMask result = MTL::ColorWriteMaskNone;
    if (has_flag(mask, RHIColourWriteMask::Red))
    {
        result |= MTL::ColorWriteMaskRed;
    }
    if (has_flag(mask, RHIColourWriteMask::Green))
    {
        result |= MTL::ColorWriteMaskGreen;
    }
    if (has_flag(mask, RHIColourWriteMask::Blue))
    {
        result |= MTL::ColorWriteMaskBlue;
    }
    if (has_flag(mask, RHIColourWriteMask::Alpha))
    {
        result |= MTL::ColorWriteMaskAlpha;
    }
    return result;
}

[[nodiscard]] inline MTL::CompareFunction to_mtl(RHICompare compare)
{
    switch (compare)
    {
    case RHICompare::Never: return MTL::CompareFunctionNever;
    case RHICompare::Less: return MTL::CompareFunctionLess;
    case RHICompare::Equal: return MTL::CompareFunctionEqual;
    case RHICompare::LessEqual: return MTL::CompareFunctionLessEqual;
    case RHICompare::Greater: return MTL::CompareFunctionGreater;
    case RHICompare::NotEqual: return MTL::CompareFunctionNotEqual;
    case RHICompare::GreaterEqual: return MTL::CompareFunctionGreaterEqual;
    case RHICompare::Always: return MTL::CompareFunctionAlways;
    }
    throw Error("RHI compare function is invalid");
}

[[nodiscard]] inline MTL::StencilOperation to_mtl(RHIStencilOp op)
{
    switch (op)
    {
    case RHIStencilOp::Keep: return MTL::StencilOperationKeep;
    case RHIStencilOp::Zero: return MTL::StencilOperationZero;
    case RHIStencilOp::Replace: return MTL::StencilOperationReplace;
    case RHIStencilOp::IncrementClamp: return MTL::StencilOperationIncrementClamp;
    case RHIStencilOp::DecrementClamp: return MTL::StencilOperationDecrementClamp;
    case RHIStencilOp::Invert: return MTL::StencilOperationInvert;
    case RHIStencilOp::IncrementWrap: return MTL::StencilOperationIncrementWrap;
    case RHIStencilOp::DecrementWrap: return MTL::StencilOperationDecrementWrap;
    }
    throw Error("RHI stencil op is invalid");
}

[[nodiscard]] inline MTL::LoadAction to_mtl(RHILoadAction action)
{
    return action == RHILoadAction::Clear ? MTL::LoadActionClear : action == RHILoadAction::Load ? MTL::LoadActionLoad : MTL::LoadActionDontCare;
}

[[nodiscard]] inline MTL::StoreAction to_mtl(RHIStoreAction action)
{
    return action == RHIStoreAction::Store ? MTL::StoreActionStore : MTL::StoreActionDontCare;
}

[[nodiscard]] inline std::string to_string(NS::String* text)
{
    return text != nullptr ? std::string(text->utf8String()) : std::string();
}

// Empty when the command buffer finished without error; call only after it completed.
[[nodiscard]] inline std::string command_buffer_failure(MTL::CommandBuffer& commands)
{
    if (commands.status() != MTL::CommandBufferStatusError)
    {
        return {};
    }
    NS::Error* error = commands.error();
    return error != nullptr ? to_string(error->localizedDescription()) : std::string("unknown GPU error");
}

template<typename T>
[[nodiscard]] NS::SharedPtr<T> require_object(NS::SharedPtr<T> object, const char* what)
{
    if (!object)
    {
        throw Error(std::string("Metal could not create ") + what);
    }
    return object;
}

inline void set_label(MTL::Resource& resource, const char* name)
{
    if (name != nullptr)
    {
        resource.setLabel(NS::String::string(name, NS::UTF8StringEncoding));
    }
}

} // namespace oryx::metal
