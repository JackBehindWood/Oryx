#include "oxpch.h"
#include "Oryx/Renderer/Batch/TextureSlotTable.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

TextureSlotTable::TextureSlotTable(RHITexturePtr white, uint32_t capacity)
    : m_slots(capacity)
{
    if (!white || capacity < 1)
    {
        throw Error("TextureSlotTable needs a white texture and at least one slot");
    }
    m_slots[0] = std::move(white);
}

uint32_t TextureSlotTable::acquire(const RHITexturePtr& texture)
{
    if (!texture)
    {
        return 0;
    }
    for (uint32_t i = 0; i < m_count; ++i)
    {
        if (m_slots[i] == texture)
        {
            return i;
        }
    }
    if (m_count == m_slots.size())
    {
        return FULL;
    }
    m_slots[m_count] = texture;
    return m_count++;
}

void TextureSlotTable::reset()
{
    for (uint32_t i = 1; i < m_count; ++i)
    {
        m_slots[i].reset();
    }
    m_count = 1;
}

} // namespace oryx
