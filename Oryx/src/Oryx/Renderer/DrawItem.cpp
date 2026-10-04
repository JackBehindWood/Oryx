#include "oxpch.h"
#include "Oryx/Renderer/DrawItem.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

void draw_item_set_vertices(DrawItem& item, const VertexBuffer& buffer, uint32_t frame_slot, uint32_t vertex_count)
{
    item.vertex_buffer = buffer.rhi_ptr();
    item.vertex_offset = buffer.offset(frame_slot);
    item.vertex_count = vertex_count;
}

void draw_item_set_indices(DrawItem& item, const IndexBuffer& buffer, uint32_t frame_slot, uint32_t index_count)
{
    item.index_buffer = buffer.rhi_ptr();
    item.index_offset = buffer.offset(frame_slot);
    item.index_type = buffer.type();
    item.index_count = index_count;
}

void draw_item_set_constants(DrawItem& item, const void* data, uint32_t size)
{
    if (size > DRAW_ITEM_MAX_CONSTANTS)
    {
        throw Error("DrawItem constants exceed the inline limit", std::to_string(size) + " > " + std::to_string(DRAW_ITEM_MAX_CONSTANTS));
    }
    std::memcpy(item.constants, data, size);
    item.constants_size = static_cast<uint16_t>(size);
}

void draw_item_add_texture(DrawItem& item, const RHITexturePtr& texture)
{
    if (item.texture_count >= DRAW_ITEM_MAX_TEXTURES)
    {
        throw Error("DrawItem has too many textures", "limit is " + std::to_string(DRAW_ITEM_MAX_TEXTURES));
    }
    item.textures[item.texture_count++] = texture;
}

} // namespace oryx
