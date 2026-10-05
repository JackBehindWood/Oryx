#pragma once

#include "Oryx/Graphics/RHI/RHIBuffer.h"
#include "Oryx/Graphics/RHI/RHIRenderState.h"
#include "Oryx/Graphics/RHI/RHISampler.h"
#include "Oryx/Graphics/RHI/RHITexture.h"
#include "Oryx/Graphics/Resources/IndexBuffer.h"
#include "Oryx/Graphics/Resources/VertexBuffer.h"
#include "Oryx/Renderer/GraphicsPipelineHandle.h"

namespace oryx
{

inline constexpr uint32_t DRAW_ITEM_MAX_CONSTANTS = 256;
inline constexpr uint32_t DRAW_ITEM_MAX_TEXTURES = RHI_MAX_TEXTURE_BINDINGS;

// One draw, submitted to the Renderer as data. Fields run from largest alignment to smallest so the struct carries no padding.
struct DrawItem
{
    RHIBufferPtr vertex_buffers[RHI_MAX_VERTEX_SLOTS];
    RHIBufferPtr index_buffer;
    RHISamplerPtr sampler;
    RHITexturePtr textures[DRAW_ITEM_MAX_TEXTURES];
    GraphicsPipelineHandle pipeline;
    uint32_t vertex_offsets[RHI_MAX_VERTEX_SLOTS] = {};
    uint32_t index_offset = 0;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    uint32_t instance_count = 1;
    uint32_t first = 0;
    uint32_t first_instance = 0;
    int32_t base_vertex = 0;
    uint16_t constants_size = 0;
    uint8_t texture_count = 0;
    IndexType index_type = IndexType::U16;
    uint8_t constants[DRAW_ITEM_MAX_CONSTANTS] = {};
};

static_assert(sizeof(DrawItem) <= 640, "DrawItem grew; keep the fields ordered by alignment");

// Binds `buffer` to a vertex slot; vertex_count is the count the draw reads (per-vertex streams).
void draw_item_set_vertices(DrawItem& item, const VertexBuffer& buffer, uint32_t frame_slot, uint32_t vertex_count, uint32_t slot = 0);
// Binds a further stream (e.g. per-instance data) without touching vertex_count.
void draw_item_set_vertex_stream(DrawItem& item, const VertexBuffer& buffer, uint32_t frame_slot, uint32_t slot);
void draw_item_set_indices(DrawItem& item, const IndexBuffer& buffer, uint32_t frame_slot, uint32_t index_count);
void draw_item_set_constants(DrawItem& item, const void* data, uint32_t size);
// Throws Error past DRAW_ITEM_MAX_TEXTURES.
void draw_item_add_texture(DrawItem& item, const RHITexturePtr& texture);

template<typename T>
void draw_item_set_constants(DrawItem& item, const T& value)
{
    draw_item_set_constants(item, &value, static_cast<uint32_t>(sizeof(T)));
}

} // namespace oryx
