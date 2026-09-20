#pragma once

#include "Oryx/Core/LayerStack.h"
#include "Oryx/Events/Event.h"

namespace oryx
{

struct ApplicationCommandLineArgs
{
    int32_t count = 0;
    char** args = nullptr;

    const char* operator[](int32_t index) const
    {
        return args[index];
    }
};

class Application
{
public:
    explicit Application(ApplicationCommandLineArgs args);
    virtual ~Application();

    void run();
    void close() { m_running = false; }

    template<typename T, typename... Args>
    T& push_layer(Args&&... args) { return m_layer_stack.push_layer<T>(std::forward<Args>(args)...); }

    template<typename T, typename... Args>
    T& push_overlay(Args&&... args) { return m_layer_stack.push_overlay<T>(std::forward<Args>(args)...); }

    void post_event(Event& event);

    static Application& Get() { return *s_instance; }

protected:
    // Runs before the layer stack sees the event, so a concrete Application (not itself a Layer) can react to it.
    virtual void on_event(Event&) {}

private:
    bool m_running = true;
    LayerStack m_layer_stack;

    static Application* s_instance;
};

// Implemented by the client application (e.g. Oasis).
Application* create_application(ApplicationCommandLineArgs args);

} // namespace oryx
