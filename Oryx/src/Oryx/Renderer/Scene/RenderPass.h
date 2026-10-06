#pragma once

#include "Oryx/Graphics/RHI/RHIRenderTarget.h"
#include "Oryx/Math/Colour.h"
#include "Oryx/Renderer/DrawItem.h"
#include "Oryx/Renderer/PipelineDef.h"

namespace oryx
{

using RenderPassId = uint32_t;

inline constexpr RenderPassId RENDER_PASS_MAIN = 0;
inline constexpr RenderPassId RENDER_PASS_NONE = std::numeric_limits<RenderPassId>::max();

// What a pass draws into and how it starts. A null `colour` is the viewport back buffer; a null `depth` is no depth attachment.
// The depth texture needs RHITextureUsage::DepthStencil and a depth format. Main's clear colour is Renderer::set_clear_colour.
struct RenderPassDesc
{
    std::string name = "Pass";
    RHIRenderTargetPtr colour;
    RHITexturePtr depth;
    RHILoadAction colour_load = RHILoadAction::Clear;
    RHIStoreAction colour_store = RHIStoreAction::Store;
    Colour clear_colour = { 0.0f, 0.0f, 0.0f, 1.0f };
    RHILoadAction depth_load = RHILoadAction::Clear;
    RHIStoreAction depth_store = RHIStoreAction::DontCare;
    float clear_depth = 1.0f;
};

// A pass's description plus this frame's draws, recorded in order and cleared once the frame is recorded.
struct RenderPass
{
    RenderPassId id = RENDER_PASS_NONE;
    RenderPassDesc desc;
    PassFormats formats;
    std::vector<DrawItem> items;
};

} // namespace oryx
