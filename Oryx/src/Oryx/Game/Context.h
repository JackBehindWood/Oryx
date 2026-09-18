#pragma once

#include "Oryx/Game/IState.h"

namespace oryx
{

class Context
{
public:
    explicit Context(IState& state) : m_state(state) {}

    [[nodiscard]] IState& state() const { return m_state; }

    template<typename Capability>
    std::type_index capability_type() const
    {
        return std::type_index(typeid(Capability));
    }

    template<typename Capability>
    [[nodiscard]] bool has() const
    {
        return has_capability(capability_type<Capability>());
    }

    template<typename Capability>
    void provide(Capability* instance)
    {
        m_capabilities[capability_type<Capability>()] = instance;
    }

    template<typename Capability>
    [[nodiscard]] Capability* get() const
    {
        auto it = m_capabilities.find(capability_type<Capability>());
        return it != m_capabilities.end() ? static_cast<Capability*>(it->second) : nullptr;
    }

    [[nodiscard]] bool has_capability(std::type_index capability) const
    {
        return m_capabilities.find(capability) != m_capabilities.end();
    }

private:
    IState& m_state;
    std::unordered_map<std::type_index, void*> m_capabilities;
};

} // namespace oryx
