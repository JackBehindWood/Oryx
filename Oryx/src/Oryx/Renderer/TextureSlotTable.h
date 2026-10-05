#pragma once

#include "Oryx/Graphics/RHI/RHITexture.h"

namespace oryx
{

// The textures of one batch; slot 0 is always the white default, so untextured primitives sample it.
class TextureSlotTable
{
public:
    static constexpr uint32_t FULL = UINT32_MAX;

    TextureSlotTable(RHITexturePtr white, uint32_t capacity);

    // Returns the existing slot for `texture` or the next free one; FULL when none is left. A null texture is the white default.
    [[nodiscard]] uint32_t acquire(const RHITexturePtr& texture);
    void reset();

    [[nodiscard]] const RHITexturePtr& texture(uint32_t slot) const { return m_slots[slot]; }
    [[nodiscard]] uint32_t count() const { return m_count; }
    [[nodiscard]] uint32_t capacity() const { return static_cast<uint32_t>(m_slots.size()); }

private:
    std::vector<RHITexturePtr> m_slots;
    uint32_t m_count = 1;
};

} // namespace oryx
