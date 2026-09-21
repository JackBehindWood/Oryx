#pragma once

#include "Oryx/Core/Log.h"
#include "Oryx/Core/Application.h"
#include "Oryx/Core/Settings.h"

extern oryx::Application* oryx::create_application(oryx::ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
    oryx::init();

    try
    {
        oryx::load_settings({ argc, argv });
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
