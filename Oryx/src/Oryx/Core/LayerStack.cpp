#include "oxpch.h"
#include "Oryx/Core/LayerStack.h"

namespace oryx
{

LayerStack::~LayerStack()
{
    for (auto it = m_layers.rbegin(); it != m_layers.rend(); ++it)
    {
        (*it)->detach();
    }
}

} // namespace oryx
