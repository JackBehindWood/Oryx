#include "oxpch.h"
#include "Oryx/Core/LayerStack.h"
#include "Oryx/Core/Error.h"

namespace oryx
{

LayerStack::~LayerStack()
{
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it)
    {
        Layer& layer = **it;
        invoke(layer, "detach", [&layer] { layer.detach(); });
    }
}

void LayerStack::update()
{
    for (LayerPtr& layer : m_layers)
    {
        if (!layer->is_disabled())
        {
            invoke(*layer, "update", [&layer] { layer->update(); });
        }
    }
}

void LayerStack::dispatch_event(Event& event)
{
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it)
    {
        if (event.handled)
        {
            break;
        }
        if (!(*it)->is_disabled())
        {
            Layer& layer = **it;
            invoke(layer, "event", [&layer, &event] { layer.event(event); });
        }
    }
}

void LayerStack::invoke(Layer& layer, std::string_view phase, const std::function<void()>& fn)
{
    try
    {
        fn();
        return;
    }
    catch (const Error& error)
    {
        error.log();
    }
    catch (const std::exception& exception)
    {
        OX_CORE_ERROR("[exception] {}", exception.what());
    }

    layer.disable();
    OX_CORE_ERROR("Layer '{}' disabled after an error in {}().", layer.name(), phase);
}

} // namespace oryx
