#pragma once

#include <string_view>
#include <vector>

namespace oryx {

struct ApplicationCommandLineArgs
{
    int count = 0;
    char** args = nullptr;

    const char* operator[](int index) const
    {
        return args[index];
    }

    std::vector<std::string_view> unpack() const
    {
        std::vector<std::string_view> result;
        result.reserve(count);
        for (int i = 0; i < count; ++i)
            result.emplace_back(args[i]);
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

protected:
    virtual void update() = 0;

private:
    bool m_running = true;
};

// Implemented by the client application (e.g. Oasis).
Application* create_application(ApplicationCommandLineArgs args);

} // namespace oryx
