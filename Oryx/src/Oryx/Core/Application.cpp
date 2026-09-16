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
}

void Application::run()
{
    while (m_running)
    {
        update();
    }
}

} // namespace oryx
