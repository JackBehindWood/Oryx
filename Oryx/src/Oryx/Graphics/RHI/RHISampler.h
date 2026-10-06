#pragma once

#include "Oryx/Graphics/RHI/RHIDeclarations.h"
#include "Oryx/Graphics/RHI/RHIResource.h"

namespace oryx
{

struct RHISamplerDesc
{
    RHIFilter min_filter = RHIFilter::Linear;
    RHIFilter mag_filter = RHIFilter::Linear;
    RHIAddressMode address_u = RHIAddressMode::Clamp;
    RHIAddressMode address_v = RHIAddressMode::Clamp;
};

class RHISampler : public RHIResource
{
public:
    [[nodiscard]] RHIFilter min_filter() const { return m_min_filter; }
    [[nodiscard]] RHIFilter mag_filter() const { return m_mag_filter; }
    [[nodiscard]] RHIAddressMode address_u() const { return m_address_u; }
    [[nodiscard]] RHIAddressMode address_v() const { return m_address_v; }

protected:
    explicit RHISampler(const RHISamplerDesc& desc)
        : RHIResource()
        , m_min_filter(desc.min_filter)
        , m_mag_filter(desc.mag_filter)
        , m_address_u(desc.address_u)
        , m_address_v(desc.address_v)
    {
    }

private:
    RHIFilter m_min_filter;
    RHIFilter m_mag_filter;
    RHIAddressMode m_address_u;
    RHIAddressMode m_address_v;
};

using RHISamplerPtr = Ref<RHISampler>;

} // namespace oryx
