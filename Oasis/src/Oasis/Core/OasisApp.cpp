#include <filesystem>
#include <string>
#include <string_view>

#include "OasisApp.h"
#include "OasisLayer.h"

#include "Oryx/Events/SimulationEvent.h"
#include "Oryx/Simulation/SimulationLayer.h"

namespace 
{

constexpr std::string_view kOpponentFlagPrefix = "--opponent=";
constexpr std::string_view kSimulateFlagPrefix = "--simulate=";
constexpr std::string_view kBenchmarkFlag = "--benchmark";

std::string parse_opponent_flag(const oryx::ApplicationCommandLineArgs& args)
{
    for (std::string_view arg : args.unpack())
    {
        if (arg.substr(0, kOpponentFlagPrefix.size()) == kOpponentFlagPrefix)
        {
            return std::string(arg.substr(kOpponentFlagPrefix.size()));
        }
    }
    return "";
}

std::string parse_simulate_flag(const oryx::ApplicationCommandLineArgs& args)
{
    for (std::string_view arg : args.unpack())
    {
        if (arg.substr(0, kSimulateFlagPrefix.size()) == kSimulateFlagPrefix)
        {
            return std::string(arg.substr(kSimulateFlagPrefix.size()));
        }
    }
    return "";
}

bool parse_benchmark_flag(const oryx::ApplicationCommandLineArgs& args)
{
    for (std::string_view arg : args.unpack())
    {
        if (arg == kBenchmarkFlag)
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

    push_layer<OasisLayer>(parse_opponent_flag(args), parse_simulate_flag(args), parse_benchmark_flag(args));
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
