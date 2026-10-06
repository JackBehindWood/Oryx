#pragma once

#include "Oryx.h"

namespace oasis
{

struct Options
{
    bool headless = false;
    bool benchmark = false;
    std::string game;
    std::string opponent;
    std::string simulate;
    std::string rhi;
};

enum class LaunchMode
{
    Simulate,
    Graphical,
    Console
};

struct LaunchPlan
{
    LaunchMode mode = LaunchMode::Console;
    // Resolved for Graphical; for Console and Simulate it is whatever was requested (possibly empty).
    std::string game;
};

// The one place that decides how Oasis starts: --simulate wins, then a window when graphics are built and wanted and the game has a graphics board. False means exit.
[[nodiscard]] bool plan_launch(const Options& options, bool graphics_built, LaunchPlan& out_plan);

// Oasis' own options; --game and --opponent come from the Board contributor.
[[nodiscard]] Options read_options(const oryx::ParsedArgs& args);

} // namespace oasis
