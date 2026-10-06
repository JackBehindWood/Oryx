#define NS_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include "MetalApi.h"

namespace oryx::metal
{

namespace
{

// Never called: fails the build when the vendored metal-cpp drops or renames an API the backend uses.
[[maybe_unused]] void probe_metal_cpp_api(MTL::Device* device, CA::MetalLayer* layer)
{
    NS::SharedPtr<MTL::CommandQueue> queue = NS::TransferPtr(device->newCommandQueue());
    NS::SharedPtr<MTL::CommandBuffer> commands = NS::RetainPtr(queue->commandBuffer());
    commands->setLabel(NS::String::string("probe", NS::UTF8StringEncoding));
    commands->commit();
    commands->waitUntilCompleted();
    MTL::CommandBufferStatus status = commands->status();
    NS::Error* error = commands->error();
    (void)status;
    (void)error;

    layer->setDevice(device);
    layer->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    layer->setFramebufferOnly(true);
    layer->setDrawableSize(CGSize{ 1.0, 1.0 });
    layer->setMaximumDrawableCount(3);
    layer->setDisplaySyncEnabled(true);
    layer->setAllowsNextDrawableTimeout(true);
    CA::MetalDrawable* drawable = layer->nextDrawable();
    MTL::Texture* texture = drawable != nullptr ? drawable->texture() : nullptr;
    (void)texture;
}

} // namespace

} // namespace oryx::metal
