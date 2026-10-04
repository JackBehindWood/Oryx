#pragma once

#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Graphics/RHI/RHIRenderTarget.h"
#include "Oryx/Math/Colour.h"

namespace oryx
{

struct RHIClear
{
    Colour colour;
    bool clear = true;
};

struct RHIColourAttachment
{
    RHIRenderTarget* target = nullptr;
    RHILoadAction load = RHILoadAction::Clear;
    RHIStoreAction store = RHIStoreAction::Store;
    Colour clear_colour;
};

// The texture needs RHITextureUsage::DepthStencil and a depth format.
struct RHIDepthAttachment
{
    RHITexture* texture = nullptr;
    RHILoadAction load = RHILoadAction::Clear;
    RHIStoreAction store = RHIStoreAction::DontCare;
    float clear_depth = 1.0f;
};

// Raw pointers: the command list retains every attachment when the pass begins.
struct RHIRenderPassDesc
{
    RHIColourAttachment colour[RHI_MAX_COLOUR_TARGETS];
    uint32_t colour_count = 0;
    RHIDepthAttachment depth;
};

} // namespace oryx
