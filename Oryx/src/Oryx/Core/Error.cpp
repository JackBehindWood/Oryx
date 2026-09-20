#include "oxpch.h"
#include "Oryx/Core/Error.h"

namespace oryx
{

void Error::log() const
{
    OX_CORE_ERROR("[{}] {}", category(), what());
    if (!m_detail.empty())
    {
        OX_CORE_ERROR("{}", m_detail);
    }
}

} // namespace oryx
