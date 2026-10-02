#pragma once

#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx
{

// Offscreen only; back buffers come from RHIViewport.
struct RHIRenderTargetDesc
{
    RHITexturePtr colour;
};

class RHIRenderTarget : public RHIResource
{
public:
    [[nodiscard]] virtual uint32_t width() const = 0;
    [[nodiscard]] virtual uint32_t height() const = 0;
    [[nodiscard]] virtual RHIFormat format() const = 0;
    // Null for a viewport back buffer.
    [[nodiscard]] virtual RHITexturePtr colour() const = 0;

protected:
    RHIRenderTarget() = default;
};

using RHIRenderTargetPtr = Ref<RHIRenderTarget>;

} // namespace oryx
