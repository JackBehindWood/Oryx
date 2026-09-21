#pragma once

#include "Oryx/Core/LayerStack.h"
#include "Oryx/Events/Event.h"

namespace oryx
{

inline bool g_initialised = false;

[[nodiscard]] inline bool is_initialised() noexcept { return g_initialised; }

void init();

// Hooks run once, in reverse registration order, from shutdown() - after the Application (and so every layer) is destroyed.
void register_shutdown_hook(std::function<void()> hook);
void shutdown();

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

    void close(int32_t exit_code = 0)
    {
        m_running = false;
        if (m_exit_code == 0)
        {
            m_exit_code = exit_code;
        }
    }

    [[nodiscard]] int32_t exit_code() const { return m_exit_code; }

    template<typename T, typename... Args>
    T& push_layer(Args&&... args) { return m_layer_stack.push_layer<T>(std::forward<Args>(args)...); }

    template<typename T, typename... Args>
    T& push_overlay(Args&&... args) { return m_layer_stack.push_overlay<T>(std::forward<Args>(args)...); }

    void post_event(Event& event);

    static Application& Get() { return *s_instance; }

protected:
    // Runs before the layer stack sees the event, so a concrete Application (not itself a Layer) can react to it.
    virtual void on_event(Event&) {}

    // Runs after LayerStack disabled a layer that threw in attach/update/event; the default keeps the application running.
    virtual void on_layer_disabled(Layer&, std::string_view /*phase*/) {}

private:
    bool m_running = true;
    int32_t m_exit_code = 0;
    LayerStack m_layer_stack;

    static Application* s_instance;
};

// Implemented by the client application (e.g. Oasis).
Application* create_application(ApplicationCommandLineArgs args);

} // namespace oryx
