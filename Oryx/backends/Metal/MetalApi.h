#pragma once

#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace oryx::metal
{

// GLFW's event loop provides no pool, so every frame-path function opens one.
class MetalAutoreleaseScope
{
public:
    MetalAutoreleaseScope()
        : m_pool(NS::TransferPtr(NS::AutoreleasePool::alloc()->init()))
    {
    }

    MetalAutoreleaseScope(const MetalAutoreleaseScope&) = delete;
    MetalAutoreleaseScope& operator=(const MetalAutoreleaseScope&) = delete;

private:
    NS::SharedPtr<NS::AutoreleasePool> m_pool;
};

} // namespace oryx::metal


#define OX_METAL_AUTORELEASE_SCOPE ::oryx::metal::MetalAutoreleaseScope OX_CONCAT(ox_metal_autorelease_scope_, __LINE__)
