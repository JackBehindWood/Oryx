#include "oxpch.h"
#include "Oryx/Graphics/RHI/Detail/RHIResourceRetainer.h"

namespace oryx
{

RHIResourceRetainer::RHIResourceRetainer(RHIResourceRetainer&& other) noexcept
    : m_retained(std::move(other.m_retained))
{
    std::memcpy(m_last_in_slot, other.m_last_in_slot, sizeof(m_last_in_slot));
    std::memcpy(m_last_binding, other.m_last_binding, sizeof(m_last_binding));
    other.clear();
}

RHIResourceRetainer& RHIResourceRetainer::operator=(RHIResourceRetainer&& other) noexcept
{
    if (this != &other)
    {
        m_retained = std::move(other.m_retained);
        std::memcpy(m_last_in_slot, other.m_last_in_slot, sizeof(m_last_in_slot));
        std::memcpy(m_last_binding, other.m_last_binding, sizeof(m_last_binding));
        other.clear();
    }
    return *this;
}

void RHIResourceRetainer::retain(RHIResource& resource)
{
    const size_t count = m_retained.size();
    const size_t window = std::min(count, SCAN_WINDOW);
    for (size_t i = count - window; i < count; ++i)
    {
        if (m_retained[i].get() == &resource)
        {
            return;
        }
    }
    m_retained.push_back(Ref<RHIResource>::from_raw(&resource));
}

void RHIResourceRetainer::retain_in_slot(uint32_t slot, RHIResource& resource)
{
    if (m_last_in_slot[slot] == &resource)
    {
        return;
    }
    retain(resource);
    m_last_in_slot[slot] = &resource;
}

void RHIResourceRetainer::retain_for_binding(RHIBindingId binding, RHIResource& resource)
{
    if (m_last_binding[binding] == &resource)
    {
        return;
    }
    retain(resource);
    m_last_binding[binding] = &resource;
}

void RHIResourceRetainer::drain_into(std::vector<Ref<RHIResource>>& sink)
{
    sink.reserve(sink.size() + m_retained.size());
    for (Ref<RHIResource>& resource : m_retained)
    {
        sink.push_back(std::move(resource));
    }
    clear();
}

void RHIResourceRetainer::clear()
{
    m_retained.clear();
    std::memset(m_last_in_slot, 0, sizeof(m_last_in_slot));
    std::memset(m_last_binding, 0, sizeof(m_last_binding));
}

} // namespace oryx
