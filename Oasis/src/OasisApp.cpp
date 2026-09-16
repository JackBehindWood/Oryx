#include <filesystem>
#include <iostream>

#include "OasisApp.h"

namespace {
constexpr int kReferenceLoopTicks = 10;
}

OasisApp::OasisApp(oryx::ApplicationCommandLineArgs args)
    : oryx::Application(args)
{
    std::cout << "Oasis — built on Oryx v"
              << oryx::VERSION_MAJOR << "."
              << oryx::VERSION_MINOR << "."
              << oryx::VERSION_PATCH << std::endl;

    ORYX_INFO("Oasis started");
    ORYX_INFO("Working directory: {}", std::filesystem::current_path().string());
}

void OasisApp::update()
{
    ORYX_INFO("Oasis tick {}", ++m_tick_count);

    if (m_tick_count >= kReferenceLoopTicks)
    {
        close();
    }
}
