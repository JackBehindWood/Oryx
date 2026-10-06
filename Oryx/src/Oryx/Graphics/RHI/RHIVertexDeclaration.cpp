#include "oxpch.h"
#include "Oryx/Graphics/RHI/RHIVertexDeclaration.h"

#include "Oryx/Core/Error.h"
#include "Oryx/Core/Fnv.h"

namespace oryx
{

namespace
{

void require_slot(uint32_t slot)
{
    if (slot >= RHI_MAX_VERTEX_SLOTS)
    {
        throw Error("RHI vertex stream slot is out of range", std::to_string(slot) + " >= " + std::to_string(RHI_MAX_VERTEX_SLOTS));
    }
}

} // namespace

const RHIVertexStream& RHIVertexDeclaration::stream(uint32_t slot) const
{
    require_slot(slot);
    return m_streams[slot];
}

RHIVertexInput RHIVertexDeclaration::input() const
{
    RHIVertexInput result;
    result.attributes = m_attributes.data();
    result.attribute_count = static_cast<uint32_t>(m_attributes.size());
    for (uint32_t slot = 0; slot < RHI_MAX_VERTEX_SLOTS; ++slot)
    {
        result.streams[slot] = m_streams[slot];
    }
    return result;
}

bool operator==(const RHIVertexDeclaration& a, const RHIVertexDeclaration& b)
{
    if (a.m_hash != b.m_hash || a.m_attributes.size() != b.m_attributes.size())
    {
        return false;
    }
    for (uint32_t slot = 0; slot < RHI_MAX_VERTEX_SLOTS; ++slot)
    {
        const RHIVertexStream& left = a.m_streams[slot];
        const RHIVertexStream& right = b.m_streams[slot];
        if (left.stride != right.stride || left.step_function != right.step_function || left.step_rate != right.step_rate)
        {
            return false;
        }
    }
    for (size_t i = 0; i < a.m_attributes.size(); ++i)
    {
        const RHIVertexAttribute& left = a.m_attributes[i];
        const RHIVertexAttribute& right = b.m_attributes[i];
        if (left.location != right.location || left.format != right.format || left.offset != right.offset || left.slot != right.slot)
        {
            return false;
        }
    }
    return true;
}

RHIVertexDeclarationBuilder& RHIVertexDeclarationBuilder::stream(uint32_t slot, uint32_t stride, RHIVertexStep step, uint32_t step_rate)
{
    require_slot(slot);
    if (stride == 0 || step_rate == 0)
    {
        throw Error("RHI vertex stream stride and step rate must be non-zero");
    }
    m_streams[slot] = { stride, step, step_rate };
    return *this;
}

RHIVertexDeclarationBuilder& RHIVertexDeclarationBuilder::attribute(uint32_t location, RHIVertexFormat format, uint32_t offset, uint32_t slot)
{
    require_slot(slot);
    m_attributes.push_back({ location, format, offset, slot });
    return *this;
}

RHIVertexDeclaration RHIVertexDeclarationBuilder::build() const
{
    for (size_t i = 0; i < m_attributes.size(); ++i)
    {
        const RHIVertexAttribute& attribute = m_attributes[i];
        const RHIVertexStream& stream = m_streams[attribute.slot];
        if (stream.stride == 0)
        {
            throw Error("RHI vertex attribute uses a stream that was not declared", "location " + std::to_string(attribute.location));
        }
        if (attribute.offset + rhi_vertex_format_bytes(attribute.format) > stream.stride)
        {
            throw Error("RHI vertex attribute does not fit its stream stride", "location " + std::to_string(attribute.location));
        }
        for (size_t j = 0; j < i; ++j)
        {
            if (m_attributes[j].location == attribute.location)
            {
                throw Error("RHI vertex attribute locations must be unique", "location " + std::to_string(attribute.location));
            }
        }
    }

    RHIVertexDeclaration result;
    result.m_attributes = m_attributes;
    for (uint32_t slot = 0; slot < RHI_MAX_VERTEX_SLOTS; ++slot)
    {
        result.m_streams[slot] = m_streams[slot];
    }
    result.finalize();
    return result;
}

void RHIVertexDeclaration::finalize()
{
    Fnv1a hash;
    m_stream_count = 0;
    for (uint32_t slot = 0; slot < RHI_MAX_VERTEX_SLOTS; ++slot)
    {
        if (m_streams[slot].stride != 0)
        {
            m_stream_count = slot + 1;
        }
        hash.mix_value(m_streams[slot].stride);
        hash.mix_value(static_cast<uint64_t>(m_streams[slot].step_function));
        hash.mix_value(m_streams[slot].step_rate);
    }
    hash.mix_value(m_attributes.size());
    for (const RHIVertexAttribute& attribute : m_attributes)
    {
        hash.mix_value(attribute.location);
        hash.mix_value(static_cast<uint64_t>(attribute.format));
        hash.mix_value(attribute.offset);
        hash.mix_value(attribute.slot);
    }
    m_hash = hash.value();
}

} // namespace oryx
