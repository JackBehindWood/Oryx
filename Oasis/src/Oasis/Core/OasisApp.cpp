#include <string>

#include "OasisApp.h"
#include "OasisLayer.h"

#include "Oryx/Events/SimulationEvent.h"
#include "Oryx/Simulation/SimulationLayer.h"

namespace 
{

constexpr std::string_view kGameFlagPrefix = "--game=";
constexpr std::string_view kOpponentFlagPrefix = "--opponent=";
constexpr std::string_view kSimulateFlagPrefix = "--simulate=";
constexpr std::string_view kBenchmarkFlag = "--benchmark";

std::string flag_value(const oryx::ApplicationCommandLineArgs& args, std::string_view prefix)
{
    for (int32_t i = 0; i < args.count; ++i)
    {
        std::string_view arg = args[i];
        if (arg.substr(0, prefix.size()) == prefix)
        {
            return std::string(arg.substr(prefix.size()));
        }
    }
    return "";
}

bool has_flag(const oryx::ApplicationCommandLineArgs& args, std::string_view flag)
{
    for (int32_t i = 0; i < args.count; ++i)
    {
        if (std::string_view(args[i]) == flag)
        {
            return true;
        }
    }
    return false;
}

} // namespace

namespace oasis
{

OasisApp::OasisApp(oryx::ApplicationCommandLineArgs args)
    : oryx::Application(args)
{
    OX_CORE_INFO("Oasis — built on Oryx v{}.{}.{}", oryx::VERSION_MAJOR, oryx::VERSION_MINOR, oryx::VERSION_PATCH);
    OX_INFO("Working directory: {}", std::filesystem::current_path().string());

    push_layer<oryx::ScriptingLayer>(oryx::script_options(args));
    push_layer<OasisLayer>(flag_value(args, kGameFlagPrefix), flag_value(args, kOpponentFlagPrefix), flag_value(args, kSimulateFlagPrefix), has_flag(args, kBenchmarkFlag));
}

void OasisApp::on_event(oryx::Event& event)
{
    oryx::EventDispatcher dispatcher(event);
    dispatcher.dispatch<oryx::StartSimulationEvent>(OX_BIND_EVENT_FN(on_start_simulation));
}

bool OasisApp::on_start_simulation(oryx::StartSimulationEvent& event)
{
    push_layer<oryx::SimulationLayer>(event.benchmark());
    return false;
}

} // namespace oasis
