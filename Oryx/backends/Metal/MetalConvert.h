#pragma once

#include "MetalApi.h"
#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx::metal
{

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
    if (has_flag(usage, RHITextureUsage::RenderTarget))
    {
        result |= MTL::TextureUsageRenderTarget;
    }
    return result;
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
