#include "oxpch.h"
#include "MetalDevice.h"

#include "MetalConvert.h"
#include "Oryx/Core/Error.h"

namespace oryx::metal
{

namespace
{

constexpr uint32_t METAL_MAX_TEXTURE_SIZE = 16384;

void upload_to_private(const MetalDevice& device, MTL::Buffer& destination, uint32_t offset, const uint8_t* data, uint32_t size)
{
    NS::SharedPtr<MTL::Buffer> staging = require_object(NS::TransferPtr(device.device()->newBuffer(data, size, MTL::ResourceStorageModeShared)), "an upload buffer");
    MTL::CommandBuffer* commands = device.command_queue()->commandBuffer();
    MTL::BlitCommandEncoder* blit = commands->blitCommandEncoder();
    blit->copyFromBuffer(staging.get(), 0, &destination, offset, size);
    blit->endEncoding();
    commands->commit();
    commands->waitUntilCompleted();
    std::string failure = command_buffer_failure(*commands);
    if (!failure.empty())
    {
        throw Error("Metal buffer upload failed", failure);
    }
}

uint64_t hash_text(const uint8_t* text, uint32_t size)
{
    uint64_t hash = 14695981039346656037ull;
    for (uint32_t i = 0; i < size; ++i)
    {
        hash = (hash ^ text[i]) * 1099511628211ull;
    }
    return hash;
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
    const bool large_argument_table = m_device->supportsFamily(MTL::GPUFamilyApple4) || m_device->supportsFamily(MTL::GPUFamilyMac2);
    m_capabilities.max_texture_bindings = std::min(RHI_MAX_TEXTURE_BINDINGS, large_argument_table ? 128u : 31u);
#ifndef OX_DIST
    m_capabilities.validates_shader_interface = true;
#endif
}

void MetalDevice::upload_buffer(MTL::Buffer& destination, uint32_t offset, const uint8_t* data, uint32_t size) const
{
    upload_to_private(*this, destination, offset, data, size);
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
            upload_to_private(*this, *buffer.get(), 0, desc.initial_data, desc.initial_data_size);
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
    const bool is_array = desc.dimension == RHITextureDimension::Tex2DArray;
    if ((desc.dimension != RHITextureDimension::Tex2D && !is_array) || desc.mip_levels != 1 || desc.array_layers == 0 || (!is_array && desc.array_layers != 1) || desc.sample_count != 1)
    {
        throw Error("Metal supports only single-level, single-sample 2D and 2D array textures");
    }
    if (has_flag(desc.usage, RHITextureUsage::DepthStencil) && !rhi_format_is_depth(desc.format))
    {
        throw Error("RHI depth-stencil texture needs a depth format");
    }
    const size_t slice_bytes = static_cast<size_t>(desc.width) * desc.height * rhi_format_bytes(desc.format);
    if (desc.initial_data_size != 0 && desc.initial_data_size != slice_bytes * desc.array_layers)
    {
        throw Error("RHI texture initial data does not match the texture size");
    }

    NS::SharedPtr<MTL::TextureDescriptor> descriptor = NS::TransferPtr(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(is_array ? MTL::TextureType2DArray : MTL::TextureType2D);
    descriptor->setArrayLength(desc.array_layers);
    descriptor->setPixelFormat(to_mtl(desc.format));
    descriptor->setWidth(desc.width);
    descriptor->setHeight(desc.height);
    descriptor->setUsage(to_mtl(desc.usage));
    const bool gpu_only = has_flag(desc.usage, RHITextureUsage::RenderTarget) || has_flag(desc.usage, RHITextureUsage::DepthStencil);
    const bool cpu_visible = desc.initial_data_size > 0 || !gpu_only;
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
        for (uint32_t layer = 0; layer < desc.array_layers; ++layer)
        {
            texture->replaceRegion(MTL::Region(0, 0, desc.width, desc.height), 0, layer, desc.initial_data + slice_bytes * layer, bytes_per_row, 0);
        }
    }
    return texture;
}

NS::SharedPtr<MTL::Library> MetalDevice::make_library(const uint8_t* text, uint32_t size)
{
    if (text == nullptr || size == 0)
    {
        throw Error("RHI shader has no code");
    }
    const uint64_t key = hash_text(text, size);
    std::unordered_map<uint64_t, NS::SharedPtr<MTL::Library>>::const_iterator cached = m_libraries.find(key);
    if (cached != m_libraries.end())
    {
        return cached->second;
    }

    NS::SharedPtr<NS::String> source = NS::TransferPtr(NS::String::alloc()->init(const_cast<uint8_t*>(text), size, NS::UTF8StringEncoding, false));
    NS::SharedPtr<MTL::CompileOptions> options = NS::TransferPtr(MTL::CompileOptions::alloc()->init());
    NS::Error* error = nullptr;
    NS::SharedPtr<MTL::Library> library = NS::TransferPtr(m_device->newLibrary(source.get(), options.get(), &error));
    if (!library)
    {
        throw Error("Metal shader compilation failed", error != nullptr ? to_string(error->localizedDescription()) : std::string("unknown compiler error"));
    }
    m_libraries.emplace(key, library);
    return library;
}

NS::SharedPtr<MTL::DepthStencilState> MetalDevice::make_depth_stencil_state(const RHIDepthStencilState& state) const
{
    NS::SharedPtr<MTL::DepthStencilDescriptor> descriptor = NS::TransferPtr(MTL::DepthStencilDescriptor::alloc()->init());
    descriptor->setDepthCompareFunction(state.depth_test ? to_mtl(state.depth_compare) : MTL::CompareFunctionAlways);
    descriptor->setDepthWriteEnabled(state.depth_write);
    if (state.stencil_test)
    {
        const RHIStencilFace* faces[2] = { &state.front, &state.back };
        for (uint32_t i = 0; i < 2; ++i)
        {
            NS::SharedPtr<MTL::StencilDescriptor> stencil = NS::TransferPtr(MTL::StencilDescriptor::alloc()->init());
            stencil->setStencilFailureOperation(to_mtl(faces[i]->fail));
            stencil->setDepthFailureOperation(to_mtl(faces[i]->depth_fail));
            stencil->setDepthStencilPassOperation(to_mtl(faces[i]->pass));
            stencil->setStencilCompareFunction(to_mtl(faces[i]->compare));
            stencil->setReadMask(state.stencil_read_mask);
            stencil->setWriteMask(state.stencil_write_mask);
            if (i == 0)
            {
                descriptor->setFrontFaceStencil(stencil.get());
            }
            else
            {
                descriptor->setBackFaceStencil(stencil.get());
            }
        }
    }
    return require_object(NS::TransferPtr(m_device->newDepthStencilState(descriptor.get())), "a depth-stencil state");
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
