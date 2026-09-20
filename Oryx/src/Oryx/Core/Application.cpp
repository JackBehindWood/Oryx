#include "oxpch.h"
#include "Oryx/Core/Application.h"

namespace oryx 
{

void init()
{
    if (g_initialised)
    {
        return;
    }

    Log::init();
    g_initialised = true;
}

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
        m_layer_stack.update();
    }
}

void Application::post_event(Event& event)
{
    on_event(event);
    if (event.handled)
    {
        return;
    }

    m_layer_stack.dispatch_event(event);
}

} // namespace oryx
