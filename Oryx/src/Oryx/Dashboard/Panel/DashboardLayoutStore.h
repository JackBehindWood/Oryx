#pragma once

#include "Oryx/Dashboard/DashboardSettings.h"
#include "Oryx/Interface/GUI/Dock/DockYaml.h"

namespace oryx
{

struct DashboardLayoutLoad
{
    gui::DockLayout layout;
    // False when the file came from a newer build: the host must not autosave over it.
    bool autosave_allowed = true;
};

// Missing file: the default, silently. Corrupt or invalid file: logged, moved aside to `<file>.bad`, the default. Newer version: logged, the default, autosave off. Never throws.
[[nodiscard]] DashboardLayoutLoad load_dashboard_layout(const DashboardSettings& settings, const gui::PanelTable& panels);

[[nodiscard]] bool save_dashboard_layout(const DashboardSettings& settings, const gui::DockLayout& layout, const gui::PanelTable& panels);

} // namespace oryx
