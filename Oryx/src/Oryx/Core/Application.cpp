#include "oxpch.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/Error.h"

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

namespace
{

std::vector<std::function<void()>>& shutdown_hooks()
{
    static std::vector<std::function<void()>> hooks;
    return hooks;
}

} // namespace

void register_shutdown_hook(std::function<void()> hook)
{
    shutdown_hooks().push_back(std::move(hook));
}

void shutdown()
{
    std::vector<std::function<void()>> pending = std::move(shutdown_hooks());
    shutdown_hooks().clear();

    for (auto it = pending.rbegin(); it != pending.rend(); ++it)
    {
        try
        {
            (*it)();
        }
        catch (const Error& error)
        {
            error.log();
        }
        catch (const std::exception& exception)
        {
            OX_CORE_ERROR("[exception] {}", exception.what());
        }
    }
}

Application* Application::s_instance = nullptr;

Application::Application(ApplicationCommandLineArgs)
{
    OX_CORE_ASSERT(!s_instance, "Application already exists!");
    s_instance = this;
    m_layer_stack.set_disabled_handler([this](Layer& layer, std::string_view phase) { on_layer_disabled(layer, phase); });
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
