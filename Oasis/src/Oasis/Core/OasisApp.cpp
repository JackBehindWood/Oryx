#include <filesystem>
#include <string>
#include <string_view>

#include "OasisApp.h"
#include "OasisLayer.h"

namespace {

constexpr std::string_view kOpponentFlagPrefix = "--opponent=";
constexpr std::string_view kSimulateFlagPrefix = "--simulate=";

// Empty means "no flag given" - OasisLayer falls back to an interactive prompt.
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

// Empty means "no flag given" - OasisLayer stays on the interactive path.
// The raw "<strategyA>,<strategyB>,<matchCount>" value is parsed/validated
// by OasisLayer itself, same division of responsibility as the opponent flag.
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

} // namespace

namespace oasis
{

OasisApp::OasisApp(oryx::ApplicationCommandLineArgs args)
    : oryx::Application(args)
{
    OX_CORE_INFO("Oasis — built on Oryx v{}.{}.{}", oryx::VERSION_MAJOR, oryx::VERSION_MINOR, oryx::VERSION_PATCH);
    OX_INFO("Working directory: {}", std::filesystem::current_path().string());

    push_layer<OasisLayer>(parse_opponent_flag(args), parse_simulate_flag(args));
}

} // namespace oasis
