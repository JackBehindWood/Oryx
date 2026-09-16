#include "Oryx/Core/EntryPoint.h"

#include "OasisApp.h"

oryx::Application* oryx::create_application(oryx::ApplicationCommandLineArgs args)
{
    return new OasisApp(args);
}
