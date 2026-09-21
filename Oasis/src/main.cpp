#include "Oryx/Core/EntryPoint.h"

#include "Oasis/Core/OasisApp.h"

OX_REGISTER_DEFAULT_SETTINGS_FILE("Oasis/oryx.yaml")

oryx::Application* oryx::create_application(oryx::ApplicationCommandLineArgs args)
{
    return new oasis::OasisApp(args);
}
