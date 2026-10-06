#include "Options.h"

namespace oasis
{

bool plan_launch(const Options& options, [[maybe_unused]] bool graphics_built, LaunchPlan& out_plan)
{
    out_plan = {};
    out_plan.game = options.game;
    if (!options.simulate.empty())
    {
        out_plan.mode = LaunchMode::Simulate;
        return true;
    }

#ifdef OX_ENABLE_GRAPHICS
    oryx::selection::FrontEnd front_end = oryx::selection::FrontEnd::Console;
    if (!oryx::selection::choose_front_end(options.game, options.headless, graphics_built, front_end, out_plan.game))
    {
        return false;
    }
    out_plan.mode = front_end == oryx::selection::FrontEnd::Graphical ? LaunchMode::Graphical : LaunchMode::Console;
#else
    out_plan.mode = LaunchMode::Console;
#endif
    return true;
}

namespace
{

class OasisCommandLine : public oryx::ICommandLineContributor
{
public:
    void declare(oryx::CommandLine& command_line) const override
    {
        command_line.flag("headless", "Run without a window or renderer (plays or simulates in the terminal)", { "console", "no-window" })
            .option("simulate", "A,B,N", "Run N matches between strategies A and B instead of playing (headless)")
            .flag("benchmark", "Report timing and memory for --simulate")
            .option("rhi", "BACKEND", "Rendering backend (e.g. metal, null)");
    }
};

OX_REGISTER_COMMAND_LINE(OasisCommandLine, "oasis")

} // namespace

Options read_options(const oryx::ParsedArgs& args)
{
    oryx::selection::BoardOptions board = oryx::selection::board_options(args);

    Options options;
    options.headless = args.has("headless");
    options.benchmark = args.has("benchmark");
    options.game = board.game;
    options.opponent = board.opponent;
    options.simulate = args.value("simulate");
    options.rhi = args.value("rhi");
    return options;
}

} // namespace oasis
