#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Events/Event.h"

namespace oryx
{

class Layer
{
public:
    explicit Layer(std::string name = "Layer");
    virtual ~Layer() = default;

    virtual void attach() {}
    virtual void detach() {}
    virtual void update() {}
    virtual void event(Event&) {}

    const std::string& name() const { return m_name; }

    [[nodiscard]] bool is_disabled() const { return m_disabled; }
    void disable() { m_disabled = true; }

private:
    std::string m_name;
    bool m_disabled = false;
};

using LayerPtr = UniquePtr<Layer>;

} // namespace oryx
