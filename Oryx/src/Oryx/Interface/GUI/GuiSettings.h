#pragma once

#include "Oryx/Core/Settings.h"
#include "Oryx/Interface/GUI/Dock/DockSolve.h"

namespace oryx
{

// The `gui:` section: where the dock layout is saved and the dock look-and-feel (dock_* keys, read live every frame). dock_preview_opacity is a percentage.
struct GuiSettings
{
    std::filesystem::path layout_file = "gui-layout.yaml";
    gui::DockStyle dock;
};

void read_settings(GuiSettings& settings, const SettingsNode& node);

} // namespace oryx
