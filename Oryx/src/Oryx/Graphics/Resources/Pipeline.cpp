#include "oxpch.h"
#include "Oryx/Graphics/Resources/Pipeline.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

Pipeline::Pipeline(RHIGraphicsPipelinePtr pipeline, std::vector<std::string> names)
    : m_pipeline(std::move(pipeline))
    , m_names(std::move(names))
{
    if (!m_pipeline)
    {
        throw Error("Pipeline requires an RHI pipeline");
    }
    if (m_names.size() != m_pipeline->binding_count())
    {
        throw Error("Pipeline needs one name per binding of the RHI pipeline");
    }
}

RHIBindingId Pipeline::try_binding(std::string_view name) const
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

RHIBindingId Pipeline::binding(std::string_view name) const
{
    const RHIBindingId id = try_binding(name);
    if (id == RHI_INVALID_BINDING)
    {
        throw Error("Pipeline has no binding named '" + std::string(name) + "'");
    }
    return id;
}

} // namespace oryx
