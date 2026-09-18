#include "oxpch.h"
#include "Oryx/Core/Application.h"

namespace oryx 
{

Application* Application::s_instance = nullptr;

Application::Application(ApplicationCommandLineArgs)
{
    OX_CORE_ASSERT(!s_instance, "Application already exists!");
    s_instance = this;
}

Application::~Application()
{
    s_instance = nullptr;
}

void Application::run()
{
    while (m_running)
    {
        for (auto& layer : m_layer_stack)
        {
            layer->update();
        }
    }
}

void Application::post_event(Event& event)
{
    for (auto it = m_layer_stack.rbegin(); it != m_layer_stack.rend(); ++it)
    {
        if (event.handled)
        {
            break;
        }
        (*it)->event(event);
    }
}

} // namespace oryx
