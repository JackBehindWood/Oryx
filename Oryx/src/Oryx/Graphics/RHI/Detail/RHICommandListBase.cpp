#include "oxpch.h"
#include "Oryx/Graphics/RHI/Detail/RHICommandListBase.h"

#include "Oryx/Core/Error.h"

namespace oryx
{

void RHICommandListBase::execute(IRHICommandContext& context) const
{
    size_t index = 0;
    for (const RHICommand& command : m_stream)
    {
        try
        {
            command.execute(context);
        }
        catch (const Error& error)
        {
            if (typeid(error) != typeid(Error))
            {
                throw;
            }
            throw Error(std::string(error.what()) + " (command #" + std::to_string(index) + " " + command.command_name() + ")", error.detail());
        }
        ++index;
    }
}

void RHICommandListBase::clear()
{
    m_stream.clear();
    m_retainer.clear();
}

void RHICommandListBase::drain_into(std::vector<Ref<RHIResource>>& sink)
{
    m_retainer.drain_into(sink);
    m_stream.clear();
}

} // namespace oryx
