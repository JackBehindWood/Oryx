#include <string>

#include "OasisApp.h"
#include "OasisLayer.h"

#include "Oryx/Scripting/ScriptSettings.h"

namespace oasis
{

OasisApp::OasisApp(oryx::ApplicationCommandLineArgs args)
    : oryx::Application(args)
{
    OX_CORE_INFO("Oasis - built on Oryx v{}.{}.{}", oryx::VERSION_MAJOR, oryx::VERSION_MINOR, oryx::VERSION_PATCH);
    OX_INFO("Working directory: {}", std::filesystem::current_path().string());

    oryx::ParsedArgs parsed = oryx::CommandLine::global().parse(args);

    if (oryx::settings_of<oryx::ScriptSettings>().enabled)
    {
        push_layer<oryx::ScriptingLayer>(oryx::script_options(parsed));
    }
    push_layer<OasisLayer>(read_options(parsed));
}

void OasisApp::on_layer_disabled(oryx::Layer&, std::string_view)
{
    close(1);
}

} // namespace oasis
