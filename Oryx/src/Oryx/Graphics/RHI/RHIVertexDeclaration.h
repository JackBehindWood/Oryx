#pragma once

#include "Oryx/Graphics/RHI/RHIRenderState.h"

namespace oryx
{

// Immutable description of every vertex stream a pipeline reads: per-stream stride, step function and rate, and the attributes in them.
// Built with RHIVertexDeclarationBuilder; equal declarations have equal hashes.
class RHIVertexDeclaration
{
public:
    RHIVertexDeclaration() { finalize(); }

    [[nodiscard]] const std::vector<RHIVertexAttribute>& attributes() const { return m_attributes; }
    [[nodiscard]] const RHIVertexStream& stream(uint32_t slot) const;
    // Highest declared slot + 1; zero for an empty declaration.
    [[nodiscard]] uint32_t stream_count() const { return m_stream_count; }
    [[nodiscard]] uint32_t stride(uint32_t slot = 0) const { return stream(slot).stride; }
    [[nodiscard]] uint64_t hash() const { return m_hash; }
    // Borrows the attribute array; valid while this declaration lives.
    [[nodiscard]] RHIVertexInput input() const;

    friend bool operator==(const RHIVertexDeclaration& a, const RHIVertexDeclaration& b);

private:
    friend class RHIVertexDeclarationBuilder;

    void finalize();

    std::vector<RHIVertexAttribute> m_attributes;
    RHIVertexStream m_streams[RHI_MAX_VERTEX_SLOTS];
    uint32_t m_stream_count = 0;
    uint64_t m_hash = 0;
};

class RHIVertexDeclarationBuilder
{
public:
    // Throws Error for a slot past RHI_MAX_VERTEX_SLOTS, a zero stride or a zero step rate.
    RHIVertexDeclarationBuilder& stream(uint32_t slot, uint32_t stride, RHIVertexStep step = RHIVertexStep::PerVertex, uint32_t step_rate = 1);
    RHIVertexDeclarationBuilder& attribute(uint32_t location, RHIVertexFormat format, uint32_t offset, uint32_t slot = 0);
    // Throws Error unless every attribute's stream is declared, the attribute fits its stride and locations are unique.
    [[nodiscard]] RHIVertexDeclaration build() const;

private:
    std::vector<RHIVertexAttribute> m_attributes;
    RHIVertexStream m_streams[RHI_MAX_VERTEX_SLOTS];
};

} // namespace oryx
