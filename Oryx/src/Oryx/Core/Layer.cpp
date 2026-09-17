#include "oxpch.h"
#include "Oryx/Core/Layer.h"

namespace oryx
{

Layer::Layer(std::string name)
    : m_name(std::move(name))
{
}

} // namespace oryx
