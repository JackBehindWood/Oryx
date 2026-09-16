#include <iostream>

#include "Oryx.h"
#include "Oryx/EntryPoint.h"

class OasisApp : public oryx::Application
{
public:
    explicit OasisApp(oryx::ApplicationCommandLineArgs args)
        : oryx::Application(args)
    {
        std::cout << "Oasis — built on Oryx v"
                  << oryx::VERSION_MAJOR << "."
                  << oryx::VERSION_MINOR << "."
                  << oryx::VERSION_PATCH << std::endl;

        ORYX_INFO("Oasis started");
    }
};

oryx::Application* oryx::create_application(oryx::ApplicationCommandLineArgs args)
{
    return new OasisApp(args);
}
