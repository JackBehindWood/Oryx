#pragma once

#include "Oryx/Core/Log.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/CommandLine.h"
#include "Oryx/Core/Settings.h"

extern oryx::Application* oryx::create_application(oryx::ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
    oryx::init();

    oryx::CommandLine command_line = oryx::CommandLine::global(argc > 0 ? argv[0] : "oryx");
    oryx::ParsedArgs parsed;
    try
    {
        parsed = command_line.parse(oryx::ApplicationCommandLineArgs{ argc, argv });
    }
    catch (const oryx::Error& error)
    {
        error.log();
        OX_CORE_INFO("\n{}", command_line.usage());
        oryx::shutdown();
        return 1;
    }

    try
    {
        oryx::load_settings(parsed);
    }
    catch (const oryx::Error& error)
    {
        error.log();
        OX_CORE_WARN("Running with default settings.");
    }

    oryx::Application* app = oryx::create_application({ argc, argv });
    app->run();
    int32_t exit_code = app->exit_code();
    delete app;
    oryx::shutdown();

    return exit_code;
}
