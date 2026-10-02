#include "oxpch.h"
#include "MetalDevice.h"

#include "MetalConvert.h"
#include "Oryx/Core/Error.h"

namespace oryx::metal
{

namespace
{

constexpr uint32_t METAL_MAX_TEXTURE_SIZE = 16384;

void upload_to_private(const MetalDevice& device, MTL::Buffer& destination, const uint8_t* data, uint32_t size)
{
    NS::SharedPtr<MTL::Buffer> staging = require_object(NS::TransferPtr(device.device()->newBuffer(data, size, MTL::ResourceStorageModeShared)), "an upload buffer");
    MTL::CommandBuffer* commands = device.command_queue()->commandBuffer();
    MTL::BlitCommandEncoder* blit = commands->blitCommandEncoder();
    blit->copyFromBuffer(staging.get(), 0, &destination, 0, size);
    blit->endEncoding();
    commands->commit();
    commands->waitUntilCompleted();
    std::string failure = command_buffer_failure(*commands);
    if (!failure.empty())
    {
        throw Error("Metal buffer upload failed", failure);
    }
}

} // namespace

MetalDevice::MetalDevice()
{
    m_device = NS::TransferPtr(MTL::CreateSystemDefaultDevice());
    if (!m_device)
    {
        throw Error("No Metal device available");
    }
    m_queue = require_object(NS::TransferPtr(m_device->newCommandQueue()), "a command queue");

    m_capabilities.name = to_string(m_device->name());
    m_capabilities.max_texture_size = METAL_MAX_TEXTURE_SIZE;
    m_capabilities.frames_in_flight = METAL_FRAMES_IN_FLIGHT;
}

NS::SharedPtr<MTL::Buffer> MetalDevice::make_buffer(const RHIBufferDesc& desc) const
{
    if (desc.size == 0)
    {
        throw Error("RHI buffer size must be non-zero");
    }
    if (desc.initial_data_size > desc.size)
    {
        throw Error("RHI buffer initial data exceeds the buffer size");
    }
    if (desc.size > m_device->maxBufferLength())
    {
        throw Error("RHI buffer size exceeds the device limit");
    }

    const bool shared = desc.memory == RHIMemory::CpuToGpu;
    NS::SharedPtr<MTL::Buffer> buffer = require_object(
        NS::TransferPtr(m_device->newBuffer(desc.size, shared ? MTL::ResourceStorageModeShared : MTL::ResourceStorageModePrivate)), "a buffer");
    set_label(*buffer.get(), desc.name);

    if (desc.initial_data_size > 0)
    {
        if (shared)
        {
            std::memcpy(buffer->contents(), desc.initial_data, desc.initial_data_size);
        }
        else
        {
            upload_to_private(*this, *buffer.get(), desc.initial_data, desc.initial_data_size);
        }
    }
    return buffer;
}

NS::SharedPtr<MTL::Texture> MetalDevice::make_texture(const RHITextureDesc& desc) const
{
    if (desc.width == 0 || desc.height == 0 || desc.width > METAL_MAX_TEXTURE_SIZE || desc.height > METAL_MAX_TEXTURE_SIZE)
    {
        throw Error("RHI texture dimensions are out of range");
    }
    const size_t byte_count = static_cast<size_t>(desc.width) * desc.height * rhi_format_bytes(desc.format);
    if (desc.initial_data_size != 0 && desc.initial_data_size != byte_count)
    {
        throw Error("RHI texture initial data does not match the texture size");
    }

    NS::SharedPtr<MTL::TextureDescriptor> descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D);
    descriptor->setPixelFormat(to_mtl(desc.format));
    descriptor->setWidth(desc.width);
    descriptor->setHeight(desc.height);
    descriptor->setUsage(to_mtl(desc.usage));
    const bool cpu_visible = desc.initial_data_size > 0 || !has_flag(desc.usage, RHITextureUsage::RenderTarget);
    if (!cpu_visible)
    {
        descriptor->setStorageMode(MTL::StorageModePrivate);
    }
    else
    {
        descriptor->setStorageMode(m_device->hasUnifiedMemory() ? MTL::StorageModeShared : MTL::StorageModeManaged);
    }

    NS::SharedPtr<MTL::Texture> texture = require_object(NS::TransferPtr(m_device->newTexture(descriptor.get())), "a texture");
    set_label(*texture.get(), desc.name);
    if (desc.initial_data_size > 0)
    {
        const NS::UInteger bytes_per_row = static_cast<NS::UInteger>(desc.width) * rhi_format_bytes(desc.format);
        texture->replaceRegion(MTL::Region(0, 0, desc.width, desc.height), 0, desc.initial_data, bytes_per_row);
    }
    return texture;
}

NS::SharedPtr<MTL::SamplerState> MetalDevice::make_sampler(const RHISamplerDesc& desc) const
{
    NS::SharedPtr<MTL::SamplerDescriptor> descriptor = NS::TransferPtr(MTL::SamplerDescriptor::alloc()->init());
    descriptor->setMinFilter(to_mtl(desc.min_filter));
    descriptor->setMagFilter(to_mtl(desc.mag_filter));
    descriptor->setSAddressMode(to_mtl(desc.address_u));
    descriptor->setTAddressMode(to_mtl(desc.address_v));
    return require_object(NS::TransferPtr(m_device->newSamplerState(descriptor.get())), "a sampler");
}

} // namespace oryx::metal
