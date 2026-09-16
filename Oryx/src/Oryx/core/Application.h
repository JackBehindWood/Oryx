#pragma once

namespace oryx {

struct ApplicationCommandLineArgs
{
    int count = 0;
    char** args = nullptr;

    const char* operator[](int index) const
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

    const ApplicationCommandLineArgs& get_command_line_args() const { return m_command_line_args; }

private:
    ApplicationCommandLineArgs m_command_line_args;
};

// Implemented by the client application (e.g. Oasis).
Application* create_application(ApplicationCommandLineArgs args);

} // namespace oryx
