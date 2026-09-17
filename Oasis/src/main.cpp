#include "Oryx/Core/EntryPoint.h"

#include "Oasis/Core/OasisApp.h"

oryx::Application* oryx::create_application(oryx::ApplicationCommandLineArgs args)
{
    return new oasis::OasisApp(args);
}
