#pragma once

#include <string_view>
#include <vector>

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

    static Application& Get() { return *s_instance; }

protected:
    virtual void update() = 0;

private:
    bool m_running = true;

    static Application* s_instance;
	friend int ::main(int argc, char** argv);
};

// Implemented by the client application (e.g. Oasis).
Application* create_application(ApplicationCommandLineArgs args);

} // namespace oryx
