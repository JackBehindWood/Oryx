#pragma once

#include "Oryx/Core/Error.h"
#include "Oryx/Graphics/RHI/IRHI.h"
#include "Oryx/Math/Colour.h"

namespace oryx
{

struct RendererDesc
{
    RHIBackend backend = default_rhi_backend();
};

// The only static renderer state: it forwards to one context that owns the device.
class Renderer
{
public:
    static void init(const RendererDesc& desc = {});
    static void shutdown();
    [[nodiscard]] static bool initialised() { return context() != nullptr; }

    [[nodiscard]] static IRHI& rhi() { return *require_context().rhi; }

    static void clear(const Colour& colour) { require_context().clears.push_back(colour); }
    static void set_viewport(RHIViewportPtr viewport) { require_context().viewport = std::move(viewport); }

    static void begin_frame() { require_context().clears.clear(); }
    static void end_frame();

private:
    struct Context
    {
        UniquePtr<IRHI> rhi;
        RHICommandList commands;
        RHIViewportPtr viewport;
        std::vector<Colour> clears;
    };

    [[nodiscard]] static UniquePtr<Context>& context()
    {
        static UniquePtr<Context> instance;
        return instance;
    }

    [[nodiscard]] static Context& require_context()
    {
        if (!initialised())
        {
            throw Error("Renderer is not initialised", "call Renderer::init first");
        }
        return *context();
    }

    static void shutdown_context(UniquePtr<Context>& context);
};

} // namespace oryx
