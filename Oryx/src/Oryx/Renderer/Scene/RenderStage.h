#pragma once

namespace oryx
{

// The fixed order a frame's stages run in; consecutive stages routed to one RenderPass share one RHI pass. Only Opaque3D..Overlay have content today.
enum class RenderStage : uint8_t
{
    Shadow,
    Opaque3D,
    Transparent3D,
    Scene2D,
    Overlay,
    PostProcess
};

inline constexpr uint32_t RENDER_STAGE_COUNT = 6;

[[nodiscard]] constexpr const char* render_stage_name(RenderStage stage)
{
    switch (stage)
    {
        case RenderStage::Shadow: return "Shadow";
        case RenderStage::Opaque3D: return "Opaque3D";
        case RenderStage::Transparent3D: return "Transparent3D";
        case RenderStage::Scene2D: return "Scene2D";
        case RenderStage::Overlay: return "Overlay";
        case RenderStage::PostProcess: return "PostProcess";
    }
    return "Unknown";
}

} // namespace oryx
