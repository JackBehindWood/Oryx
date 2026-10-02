#pragma once

#include "MetalApi.h"
#include "Oryx/Graphics/RHI/RHICapabilities.h"
#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx::metal
{

inline constexpr uint32_t METAL_FRAMES_IN_FLIGHT = 3;

// Owns the MTLDevice and its one command queue; allocation is validated here so every backend resource shares one set of rules.
class MetalDevice
{
public:
    MetalDevice();

    [[nodiscard]] MTL::Device* device() const { return m_device.get(); }
    [[nodiscard]] MTL::CommandQueue* command_queue() const { return m_queue.get(); }
    [[nodiscard]] const RHICapabilities& capabilities() const { return m_capabilities; }

    [[nodiscard]] NS::SharedPtr<MTL::Buffer> make_buffer(const RHIBufferDesc& desc) const;
    [[nodiscard]] NS::SharedPtr<MTL::Texture> make_texture(const RHITextureDesc& desc) const;
    [[nodiscard]] NS::SharedPtr<MTL::SamplerState> make_sampler(const RHISamplerDesc& desc) const;

private:
    NS::SharedPtr<MTL::Device> m_device;
    NS::SharedPtr<MTL::CommandQueue> m_queue;
    RHICapabilities m_capabilities;
};

} // namespace oryx::metal
