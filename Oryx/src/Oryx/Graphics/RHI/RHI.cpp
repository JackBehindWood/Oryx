#include "oxpch.h"
#include "Oryx/Graphics/RHI/IRHI.h"

#include "Oryx/Core/Error.h"
#include "NullRHI.h"

namespace oryx
{

namespace
{

[[noreturn]] void not_implemented(RHIBackend backend)
{
    throw Error(to_string(backend) + " is not implemented");
}

} // namespace

std::string to_string(RHIBackend backend)
{
    switch (backend)
    {
    case RHIBackend::Null: return "Null";
    case RHIBackend::Metal: return "Metal";
    case RHIBackend::OpenGL: return "OpenGL";
    case RHIBackend::Vulkan: return "Vulkan";
    case RHIBackend::D3D12: return "D3D12";
    case RHIBackend::WebGPU: return "WebGPU";
    }
    return "Unknown";
}

RHIBackend parse_rhi_backend(std::string_view str)
{
    std::string lowered(str);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    constexpr std::array<RHIBackend, 6> all = { RHIBackend::Null, RHIBackend::Metal, RHIBackend::OpenGL, RHIBackend::Vulkan, RHIBackend::D3D12, RHIBackend::WebGPU };
    for (RHIBackend backend : all)
    {
        std::string name = to_string(backend);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (name == lowered)
        {
            return backend;
        }
    }
    throw Error("Unknown RHI backend '" + std::string(str) + "'");
}

RHIBackend default_rhi_backend()
{
#ifdef OX_PLATFORM_MACOS
    return RHIBackend::Metal;
#else
    return RHIBackend::Null;
#endif
}

void rhi_validate_present_source(const RHIViewport& viewport, const RHITexture& source)
{
    if (!has_flag(source.usage(), RHITextureUsage::Sampled))
    {
        throw Error("RHI present source was not created with RHITextureUsage::Sampled");
    }
    if (source.width() != viewport.width() || source.height() != viewport.height() || source.format() != viewport.format())
    {
        throw Error("RHI present source does not match the viewport size and format");
    }
}

UniquePtr<IRHI> create_rhi(RHIBackend backend)
{
    switch (backend)
    {
    case RHIBackend::Null: return create_unique<NullRHI>();
    case RHIBackend::Metal: not_implemented(backend);
    case RHIBackend::OpenGL: not_implemented(backend);
    case RHIBackend::Vulkan: not_implemented(backend);
    case RHIBackend::D3D12: not_implemented(backend);
    case RHIBackend::WebGPU: not_implemented(backend);
    }
    throw Error("Invalid RHI backend");
}

} // namespace oryx
