#pragma once

#include "Oryx/Containers/Pair.h"
#include "Oryx/Containers/SmallVector.h"
#include "Oryx/Game/IState.h"

namespace oryx
{

constexpr size_t kContextInlineCapabilities = 4;

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
        std::type_index type = capability_type<Capability>();
        for (Entry& entry : m_capabilities)
        {
            if (entry.key == type)
            {
                entry.value = instance;
                return;
            }
        }
        m_capabilities.push_back(Entry{ type, instance });
    }

    template<typename Capability>
    [[nodiscard]] Capability* get() const
    {
        const Entry* entry = find_capability(capability_type<Capability>());
        return entry != nullptr ? static_cast<Capability*>(entry->value) : nullptr;
    }

    [[nodiscard]] bool has_capability(std::type_index capability) const
    {
        return find_capability(capability) != nullptr;
    }

private:
    using Entry = Pair<std::type_index, void*>;

    [[nodiscard]] const Entry* find_capability(std::type_index capability) const
    {
        for (const Entry& entry : m_capabilities)
        {
            if (entry.key == capability)
            {
                return &entry;
            }
        }
        return nullptr;
    }

    IState& m_state;
    SmallVector<Entry, kContextInlineCapabilities> m_capabilities;
};

} // namespace oryx
