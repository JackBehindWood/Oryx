#pragma once

#include "Oryx/Core/Error.h"

namespace oryx
{

// Validity token for engine objects lent to a script for one call; handles check it on every use and a LeaseScope arms then always revokes it.
class ScriptLease
{
public:
    [[nodiscard]] bool valid() const { return m_valid; }
    void arm() { m_valid = true; }
    void revoke() { m_valid = false; }

    void require(std::string_view what) const
    {
        if (!m_valid)
        {
            throw Error("this " + std::string(what) + " is no longer valid: it was only lent to the strategy for the duration of decide()");
        }
    }

private:
    bool m_valid = false;
};

class LeaseScope
{
public:
    explicit LeaseScope(ScriptLease& lease)
        : m_lease(lease)
    {
        m_lease.arm();
    }

    ~LeaseScope() { m_lease.revoke(); }

    LeaseScope(const LeaseScope&) = delete;
    LeaseScope& operator=(const LeaseScope&) = delete;

private:
    ScriptLease& m_lease;
};

} // namespace oryx
