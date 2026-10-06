#pragma once

#include "Oryx/Graphics/RHI/RHIRenderTarget.h"
#include "Oryx/Graphics/Resources/Texture2D.h"

namespace oryx
{

// An offscreen colour target whose texture can be sampled.
class RenderTarget
{
public:
    RenderTarget(RHIRenderTargetPtr target, Texture2D colour);

    static RenderTarget create(IRHI& rhi, uint32_t width, uint32_t height, RHIFormat format = RHIFormat::RGBA8Unorm);

    [[nodiscard]] RHIRenderTarget& rhi() const { return *m_target; }
    [[nodiscard]] const RHIRenderTargetPtr& rhi_ptr() const { return m_target; }
    [[nodiscard]] const Texture2D& colour() const { return m_colour; }
    [[nodiscard]] uint32_t width() const { return m_target->width(); }
    [[nodiscard]] uint32_t height() const { return m_target->height(); }

private:
    RHIRenderTargetPtr m_target;
    Texture2D m_colour;
};

} // namespace oryx
