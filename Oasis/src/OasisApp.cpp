#include <filesystem>

#include "OasisApp.h"
#include "OasisLayer.h"

OasisApp::OasisApp(oryx::ApplicationCommandLineArgs args)
    : oryx::Application(args)
{
    OX_CORE_INFO("Oasis — built on Oryx v{}.{}.{}", oryx::VERSION_MAJOR, oryx::VERSION_MINOR, oryx::VERSION_PATCH);
    OX_INFO("Working directory: {}", std::filesystem::current_path().string());

    push_layer<OasisLayer>();
}
