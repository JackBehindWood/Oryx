#include "oxpch.h"
#include "Oryx/Core/Application.h"

namespace oryx {

Application::Application(ApplicationCommandLineArgs args)
    : m_command_line_args(args)
{
}

Application::~Application()
{
}

void Application::run()
{
}

} // namespace oryx
