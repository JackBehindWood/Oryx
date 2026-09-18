#pragma once

#include "Oryx/Core/LayerStack.h"
#include "Oryx/Events/Event.h"

#include <string_view>

int main(int argc, char** argv);

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

    std::vector<std::string_view> unpack() const
    {
        std::vector<std::string_view> result;
        result.reserve(count);
        for (int32_t i = 0; i < count; ++i)
        {
            result.emplace_back(args[i]);
        }
        return result;
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

private:
    bool m_running = true;
    LayerStack m_layer_stack;

    static Application* s_instance;
	friend int ::main(int argc, char** argv);
};

// Implemented by the client application (e.g. Oasis).
Application* create_application(ApplicationCommandLineArgs args);

} // namespace oryx
