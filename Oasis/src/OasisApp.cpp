#include <filesystem>

#include "OasisApp.h"

namespace {
constexpr int kReferenceLoopTicks = 10;
}

OasisApp::OasisApp(oryx::ApplicationCommandLineArgs args)
    : oryx::Application(args)
{
    ORYX_CORE_INFO("Oasis — built on Oryx v{}.{}.{}", oryx::VERSION_MAJOR, oryx::VERSION_MINOR, oryx::VERSION_PATCH);
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
