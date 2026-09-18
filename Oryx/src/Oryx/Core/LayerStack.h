#pragma once

#include "Oryx/Core/Base.h"
#include "Oryx/Core/Layer.h"

namespace oryx
{

class LayerStack
{
public:
    LayerStack() = default;
    ~LayerStack();

    template<typename T, typename... Args>
    T& push_layer(Args&&... args)
    {
        static_assert(std::is_base_of_v<Layer, T>, "T must derive from oryx::Layer");
        UniquePtr<T> layer = create_unique<T>(std::forward<Args>(args)...);
        T& ref = *layer;
        m_layers.emplace(m_layers.begin() + m_layer_insert_index, std::move(layer));
        ++m_layer_insert_index;
        ref.attach();
        return ref;
    }

    template<typename T, typename... Args>
    T& push_overlay(Args&&... args)
    {
        static_assert(std::is_base_of_v<Layer, T>, "T must derive from oryx::Layer");
        UniquePtr<T> overlay = create_unique<T>(std::forward<Args>(args)...);
        T& ref = *overlay;
        m_layers.emplace_back(std::move(overlay));
        ref.attach();
        return ref;
    }

    std::vector<LayerPtr>::iterator begin() { return m_layers.begin(); }
    std::vector<LayerPtr>::iterator end() { return m_layers.end(); }
    std::vector<LayerPtr>::reverse_iterator rbegin() { return m_layers.rbegin(); }
    std::vector<LayerPtr>::reverse_iterator rend() { return m_layers.rend(); }

private:
    std::vector<LayerPtr> m_layers;
    size_t m_layer_insert_index = 0;
};

} // namespace oryx
