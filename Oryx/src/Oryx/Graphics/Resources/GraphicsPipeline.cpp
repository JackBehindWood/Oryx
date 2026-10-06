#include "oxpch.h"
#include "Oryx/Graphics/Resources/GraphicsPipeline.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

GraphicsPipeline::GraphicsPipeline(RHIGraphicsPipelinePtr pipeline, std::vector<std::string> names, bool fallback)
    : m_pipeline(std::move(pipeline))
    , m_names(std::move(names))
    , m_fallback(fallback)
{
    if (!m_pipeline)
    {
        throw Error("GraphicsPipeline requires an RHI pipeline");
    }
    if (m_names.size() != m_pipeline->binding_count())
    {
        throw Error("GraphicsPipeline needs one name per binding of the RHI pipeline");
    }
}

RHIBindingId GraphicsPipeline::try_binding(std::string_view name) const
{
    for (size_t i = 0; i < m_names.size(); ++i)
    {
        if (m_names[i] == name)
        {
            return static_cast<RHIBindingId>(i);
        }
    }
    return RHI_INVALID_BINDING;
}

RHIBindingId GraphicsPipeline::binding(std::string_view name) const
{
    const RHIBindingId id = try_binding(name);
    if (id == RHI_INVALID_BINDING)
    {
        throw Error("GraphicsPipeline has no binding named '" + std::string(name) + "'");
    }
    return id;
}

} // namespace oryx
