#pragma once

#include "Oryx/Graphics/RHI/RHIFormat.h"
#include "Oryx/Graphics/RHI/RHIResource.h"
#include "Oryx/Graphics/RHI/RHIShader.h"

namespace oryx
{

enum class RHIVertexFormat : uint8_t
{
    Float,
    Float2,
    Float3,
    Float4
};

enum class RHIBlend : uint8_t
{
    Opaque,
    Alpha
};

struct RHIVertexAttribute
{
    uint32_t location = 0;
    RHIVertexFormat format = RHIVertexFormat::Float3;
    uint32_t offset = 0;
    uint32_t slot = 0;
};

// Shaders and attributes are only read during creation; the pipeline retains neither.
struct RHIGraphicsPipelineDesc
{
    RHIVertexShaderPtr vertex;
    RHIPixelShaderPtr pixel;
    const RHIVertexAttribute* attributes = nullptr;
    uint32_t attribute_count = 0;
    uint32_t vertex_stride = 0;
    RHIFormat colour_format = RHIFormat::BGRA8Unorm;
    RHIBlend blend = RHIBlend::Opaque;
};

// Common base of graphics and (later) compute pipelines; holds nothing.
class RHIPipeline : public RHIResource
{
protected:
    RHIPipeline() = default;
};

class RHIGraphicsPipeline : public RHIPipeline
{
public:
    [[nodiscard]] RHIFormat colour_format() const { return m_colour_format; }
    [[nodiscard]] RHIBlend blend() const { return m_blend; }

protected:
    explicit RHIGraphicsPipeline(const RHIGraphicsPipelineDesc& desc)
        : m_colour_format(desc.colour_format)
        , m_blend(desc.blend)
    {
    }

private:
    RHIFormat m_colour_format;
    RHIBlend m_blend;
};

using RHIGraphicsPipelinePtr = Ref<RHIGraphicsPipeline>;

} // namespace oryx
