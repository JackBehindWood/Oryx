#include "oxpch.h"
#include "Oryx/Core/Application.h"

namespace oryx {

Application::Application(ApplicationCommandLineArgs)
{
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
