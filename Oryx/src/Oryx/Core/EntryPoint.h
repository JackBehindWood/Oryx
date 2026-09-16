#pragma once

#include "Oryx/Core/Log.h"
#include "Oryx/Core/Application.h"

extern oryx::Application* oryx::create_application(oryx::ApplicationCommandLineArgs args);

int main(int argc, char** argv)
{
    oryx::Log::init();

    oryx::Application* app = oryx::create_application({ argc, argv });
    app->run();
    delete app;

    return 0;
}
